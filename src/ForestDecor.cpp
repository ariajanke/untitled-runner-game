/****************************************************************************

    Copyright 2021 Aria Janke

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*****************************************************************************/

#include "ForestDecor.hpp"
#include "maps/LineMapLoader.hpp"
#include "maps/MapObjectLoader.hpp"

#include <tmap/TilePropertiesInterface.hpp>
#include <tmap/TiledMap.hpp>

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>

#include <cassert>

#include <iostream>
#include <thread>
#include <mutex>
#include <future>

namespace {

using GroundsClassMap = std::unordered_map<int, std::vector<bool>>;

struct WfStripInfo {
    static constexpr const int k_uninit = -1;
    int step     = k_uninit;
    int reset_id = k_uninit;
    int min      = k_uninit;
    int max      = k_uninit;
};

class WfFramesInfo final : public ForestDecor::Updatable {
public:
    using ConstTileSetPtr = tmap::TiledMap::ConstTileSetPtr;
    using TileEffect = tmap::TileEffect;

    WfFramesInfo() {}

    void setup(const std::vector<WfStripInfo> & strips, ConstTileSetPtr tsptr);

    int get_new_tid(int num_of_same_above) const;

    template <typename Func>
    void for_each_tid_and_tile_effect(Func && f);

    void update(double et) override;

private:
    class WfEffect final : public TileEffect {
    public:
        void assign_time(const double & time) { m_time_ptr = &time; }

        void setup_frames(const std::vector<int> & local_ids, ConstTileSetPtr tsptr);

    private:
        using DrawOnlyTarget = tmap::DrawOnlyTarget;

        void operator () (sf::Sprite & spt, DrawOnlyTarget & target) override;

        std::vector<sf::IntRect> m_frames;
        const double * m_time_ptr = nullptr;
    };

    void check_invarients() const;

    double m_time = 0.;
    std::size_t m_reset_y = 0;
    std::vector<WfEffect> m_effects;
    std::vector<int> m_effect_tids;
};

struct ForestLoadTemp final : public MapDecorDrawer::TempRes {
    // THIS is fine actually
    std::map<int, std::shared_ptr<WfFramesInfo>> gid_to_strips;
};

} // end of <anonymous> namespace

static GroundsClassMap load_grounds_map
    (const tmap::TilePropertiesInterface & layer, const LineMapLoader::SegmentMap & segments_map);

// multithreaded version
static std::unique_ptr<ForestDecor::FutureTreeMaker> make_future_tree_maker();

// single thread version
static std::unique_ptr<ForestDecor::FutureTreeMaker> make_syncro_tree_maker();

static std::shared_ptr<WfFramesInfo> load_new_strip
    (const tmap::TiledMap & tmap, int gid, const std::string & value_string);

ForestDecor::~ForestDecor() {
    int i = 0;
    for (const auto & tree : m_trees) {
        tree.save_to_file("/media/ramdisk/tree-" + std::to_string(i++) + ".png");
    }
}

void ForestDecor::render_front(sf::RenderTarget & target) const {
    Rect draw_bounds(VectorD(target.getView().getCenter() - target.getView().getSize()*0.5f),
                     VectorD(target.getView().getSize()));
#   if 0
    for (const auto & tree : m_trees) {
        if (!draw_bounds.intersects(tree.bounding_box())) continue;
        tree.render_fronts(target, sf::RenderStates::Default);
    }
#   endif
}

void ForestDecor::render_back(sf::RenderTarget & target) const {
    Rect draw_bounds(VectorD(target.getView().getCenter() - target.getView().getSize()*0.5f),
                     VectorD(target.getView().getSize()));
    for (const auto & flower : m_flowers) {
        Rect flower_bounds(flower.location(), VectorD(flower.width(), flower.height()));
        if (!draw_bounds.intersects(flower_bounds)) continue;
        target.draw(flower);
    }
#   if 0
    for (const auto & tree : m_trees) {
        // note: no culling
        if (!draw_bounds.intersects(tree.bounding_box())) continue;
        tree.render_backs(target, sf::RenderStates::Default);
    }
#   endif
}

void ForestDecor::update(double et) {
    for (auto & flower : m_flowers) {
        flower.update(et);
    }
    for (auto & [fut_tree_ptr, e] : m_future_trees) {
        if (fut_tree_ptr->is_ready()) {
            auto & new_tree = m_trees.emplace_back(fut_tree_ptr->get_tree());
            // "asserted" to work
            auto * script = dynamic_cast<LeavesDecorScript *>(get_script(e));
            if (script)
                script->inform_of_front_leaves(new_tree.front_leaves_bitmap());
        }
    }
    {
    auto end = m_future_trees.end();
    static auto tree_is_done = [](const std::pair<std::unique_ptr<FutureTree>, Entity> & pair)
        { return pair.first->is_done(); };
    m_future_trees.erase(
        std::remove_if(m_future_trees.begin(), end, tree_is_done),
        end);
    }

    for (auto & sptr : m_updatables) {
        sptr->update(et);
    }
}

/* private */ std::unique_ptr<ForestDecor::TempRes> ForestDecor::prepare_map_objects
    (const tmap::TiledMap & tmap, MapObjectLoader & objloader)
{
    load_map_vegetation(tmap, objloader);
    // there needs to be a better way to handle temporaries!
    // can I alleviate this to some degree with double dispatch? or something else?
    return load_map_waterfalls(tmap);
}

/* private */ void ForestDecor::prepare_map
    (tmap::TiledMap & tmap, std::unique_ptr<ForestDecor::TempRes> resptr)
{
    auto & gid_to_strips = dynamic_cast<ForestLoadTemp &>(*resptr).gid_to_strips;

    auto get_wf_ptr = [&gid_to_strips](int gid) -> std::shared_ptr<WfFramesInfo> {
        auto itr = gid_to_strips.find(gid);
        return itr == gid_to_strips.end() ? nullptr : itr->second;
    };

    for (const auto * layer : tmap) {
        if (!tmap.convert_to_writable_tile_layer(layer)) continue;
        auto & map_tiles = *tmap.convert_to_writable_tile_layer(layer);
        std::shared_ptr<const WfFramesInfo> ptr = nullptr;
        // order of dimensions important here
        for (int x = 0; x != map_tiles.width (); ++x) {
            // must reset per column
            int count = 0;
            for (int y = 0; y != map_tiles.height(); ++y) {
                int gid = map_tiles.tile_gid(x, y);
                if (ptr != get_wf_ptr(gid)) {
                    count = 0;
                    ptr = get_wf_ptr(gid);
                }

                if (ptr) {
                    // if it's a magic tile, there must be a tileset associated with it
                    auto tsptr = tmap.get_tile_set_for_gid(gid);
                    assert(tsptr);
                    int new_tid = ptr->get_new_tid( count++ );
                    map_tiles.set_tile_gid(x, y, tsptr->convert_to_gid( new_tid ));
                }
            }
        }
    }

    for (auto & pair : gid_to_strips) {
        using TileEffect = tmap::TileEffect;
        auto tsptr = tmap.get_tile_set_for_gid(pair.first);
        pair.second->for_each_tid_and_tile_effect([tsptr](int tid, TileEffect & te) {
            tsptr->set_effect(tid, &te);
        });
        // proper transfer ownership to instance
        m_updatables.emplace(pair.second);
    }

    // ensure all magic tiles are replaced
#   ifdef MACRO_DEBUG
    for (const auto * layer : tmap) {
        if (!tmap.convert_to_writable_tile_layer(layer)) continue;
        auto & map_tiles = *tmap.convert_to_writable_tile_layer(layer);

        // order of dimensions important here
        for (int x = 0; x != map_tiles.width (); ++x) {
        for (int y = 0; y != map_tiles.height(); ++y) {
            assert(gid_to_strips.find(map_tiles.tile_gid(x, y)) == gid_to_strips.end());
        }}
    }
#   endif
}

/* private */ void ForestDecor::load_map_vegetation(const tmap::TiledMap & tmap, MapObjectLoader & objloader) {
    if (k_use_multithreaded_tree_loading)
        { m_tree_maker = make_future_tree_maker(); }
    else
        { m_tree_maker = make_syncro_tree_maker(); }

    auto * ground = tmap.find_tile_layer("ground");
    if (!ground) return;
    auto tile_size   = LineMapLoader::load_tile_size(tmap);
    auto segments    = LineMapLoader::load_tileset_map(tmap, tile_size.width, tile_size.height);
    const auto grounds_map = load_grounds_map(*ground, segments.segment_map);
    static constexpr const int k_seed = 0xDEADBEEF;
    std::default_random_engine rng { k_seed };
    for (int y = 0; y != ground->height(); ++y) {
    for (int x = 0; x != ground->width (); ++x) {
        auto itr = grounds_map.find(ground->tile_gid(x, y));
        if (itr == grounds_map.end()) continue;
        bool any_are_ground = std::any_of(itr->second.begin(), itr->second.end(), [](bool b) { return b; });
        if (!any_are_ground) continue;
        if (std::uniform_int_distribution<int>(0, 3)(rng) != 0) continue;
        std::size_t med = [](const std::vector<bool> & bools) {
            auto beg = bools.begin();
            while (!*beg) {
                ++beg;
                assert(beg != bools.end());
            }
            auto end = beg;
            while (end != bools.end()) {
                if (!*end) break;
                ++end;
            }
            return (end - beg) / 2;
        } (itr->second);
        auto target_seg = segments.segment_map.find(ground->tile_gid(x, y))->second.segments[med];
        target_seg = move_segment(target_seg, VectorD(double(x)*tile_size.width, double(y)*tile_size.height));

        auto plant_location = (target_seg.a + target_seg.b)*0.5;

        auto plflower = &ForestDecor::plant_new_flower;
        auto pltree   = &ForestDecor::plant_new_future_tree;
        auto fp = choose_random(rng, { plflower, plflower, plflower, plflower, plflower, plflower, pltree });

        std::invoke(fp, *this, rng, plant_location, objloader);
    }}
}

/* private */ std::unique_ptr<ForestDecor::TempRes> ForestDecor::load_map_waterfalls(const tmap::TiledMap & tmap) {
    // I need to seperate loading information on "magic" link tiles
    // and then modifying the actual map tiles
    // so... maybe this function can "just load" information on this linked tiles
    // and then another chain of calls which will modify the map following a load

    auto rv = std::make_unique<ForestLoadTemp>();
    std::map<int, std::shared_ptr<WfFramesInfo>> & gid_to_strips = rv->gid_to_strips;
    for (const sf::Drawable * layer : tmap) {
        const auto * tile_layer = dynamic_cast<const tmap::TilePropertiesInterface *>(layer);
        if (!tile_layer) continue;
        for (int x = 0; x != tile_layer->width (); ++x) {
        for (int y = 0; y != tile_layer->height(); ++y) {
            auto wf_itr_ptr = (*tile_layer)(x, y);

            if (!wf_itr_ptr) continue;
            auto gid = tile_layer->tile_gid(x, y);
            make_do_if_found(*wf_itr_ptr)("animation-falls", [gid, &gid_to_strips, &tmap]
                        (const std::string & val)
            {
                auto itr = gid_to_strips.find(gid);
                // if already initialized... skip
                if (itr == gid_to_strips.end()) {
                    gid_to_strips[gid] = load_new_strip(tmap, gid, val);
                }
            });
        }}
    }

    return rv;
}

/* private */ void ForestDecor::plant_new_flower(std::default_random_engine & rng, VectorD plant_location, MapObjectLoader &) {
    Flower flower;
    flower.setup(rng);
    flower.set_location(plant_location - VectorD(flower.width()*0.5, flower.height()));
    m_flowers.emplace_back(flower);
}

/* private */ void ForestDecor::plant_new_future_tree
    (std::default_random_engine & rng, VectorD location, MapObjectLoader & objloader)
{
    TreeParameters params(location, rng);

    auto e = objloader.create_entity();
#   if 0
    auto script = std::make_unique<LeavesDecorScript>();
    auto & rect = e.add<PhysicsComponent>().reset_state<Rect>();
    std::tie(rect.left, rect.top) = as_tuple(PlantTree::leaves_location_from_params(params, location));
    rect.width  = params.leaves_size.width;
    rect.height = params.leaves_size.height;
    e.add<TriggerBox>();
    e.add<ScriptUPtr>() = std::move(script);
#   endif
    m_future_trees.emplace_back(std::make_pair(m_tree_maker->make_tree(params), e));
}

namespace {

using StrCItr = std::string::const_iterator;

void WfFramesInfo::setup(const std::vector<WfStripInfo> & strips, ConstTileSetPtr tsptr) {
    // need to enforce that strips are all the same size and form a grid
    // (perhaps using a Grid would be more apporpiate?)
    std::vector<int> frame_ids;
    frame_ids.reserve(strips.size());
    for (const auto & strip : strips) {
        frame_ids.push_back(strip.min);
    }

    if (frame_ids.empty()) return; // got nothing anyway
    static auto count_steps = [](const WfStripInfo & strip)
        { return ((strip.max - strip.min) / strip.step) + 1; };
    {
    static constexpr auto k_uninit = WfStripInfo::k_uninit;
    int steps = k_uninit;
    for (const auto & strip : strips) {
        if (steps == k_uninit) {
            steps = count_steps(strip);
        } else if (steps != count_steps(strip)) {
            throw std::runtime_error("uneven strips, each strip must have the same number of steps.");
        }
    }
    }
    // all stop on first reset!
    for (bool more_remain = true; more_remain; ) {
        WfEffect new_effect;
        new_effect.assign_time(m_time);
        new_effect.setup_frames(frame_ids, tsptr);
        m_effect_tids.push_back(frame_ids.front());
        m_effects.emplace_back(std::move(new_effect));

        // step here:
        for (auto & id : frame_ids) {
            const auto & strip = strips[ &id - &frame_ids.front() ];
            if (id < strip.reset_id && id + strip.step >= strip.reset_id) {
                assert(m_reset_y == 0 || m_reset_y == m_effects.size());
                m_reset_y = m_effects.size();
            }
            id += strip.step;
            if (id > strip.max) {
                more_remain = false;
                break;
            }
        }
    }
    check_invarients();
}

int WfFramesInfo::get_new_tid(int num_of_same_above) const {
    assert(num_of_same_above > -1);
    auto seqnum = std::size_t(num_of_same_above);
    if (seqnum >= m_effects.size()) {
        seqnum = ((seqnum - m_effects.size()) % (m_effects.size() - m_reset_y)) + m_reset_y;
        assert(seqnum < m_effects.size());
    }
    return m_effect_tids.at(seqnum);
}

template <typename Func>
void WfFramesInfo::for_each_tid_and_tile_effect(Func && f) {
    for (auto & wfte : m_effects) {
        TileEffect & te = wfte;
        int gid = m_effect_tids[&wfte - &m_effects.front()];
        f(gid, te);
    }
}

void WfFramesInfo::update(double et) {
    m_time = std::fmod(m_time + et, 1.);
    check_invarients();
}

/* private */ void WfFramesInfo::check_invarients() const {
    assert(m_reset_y <= m_effects.size());
    assert(m_time >= 0. && m_time <= 1.);
    assert(m_effects.size() == m_effect_tids.size());
}

void WfFramesInfo::WfEffect::setup_frames(const std::vector<int> & local_ids, ConstTileSetPtr tsptr) {
    m_frames.reserve(local_ids.size());
    for (int id : local_ids) {
        m_frames.push_back( tsptr->texture_rectangle(id) );
    }
}

/* private */ void WfFramesInfo::WfEffect::operator () (sf::Sprite & spt, DrawOnlyTarget & target) {
    assert(m_time_ptr);
    assert( *m_time_ptr >= 0. && *m_time_ptr <= 1. );
    auto idx = std::size_t( std::floor(double( m_frames.size() )*(*m_time_ptr)) );
    const auto & frame = m_frames[idx];
    // expected values are in [0 width]
    int x_offset = std::round(double(frame.width)*(*m_time_ptr));
    if (x_offset == 0 || x_offset == frame.width) {
        spt.setTextureRect(frame);
        target.draw(spt);
    } else {
        auto second_frame = frame;
        second_frame.left  += (frame.width - x_offset);
        second_frame.width -= (frame.width - x_offset);
        spt.setTextureRect(second_frame);
        target.draw(spt);

        auto first_frame = frame;
        first_frame.width -= x_offset;
        spt.setTextureRect(first_frame);
        spt.move(float(x_offset), 0.f);
        target.draw(spt);
    }
}

} // end of <anonymous> namespace

inline static bool is_comma(char c) { return c == ','; }

static GroundsClassMap load_grounds_map
    (const tmap::TilePropertiesInterface & layer, const LineMapLoader::SegmentMap & segments_map)
{
    GroundsClassMap rv;
    for (int y = 0; y != layer.height(); ++y) {
    for (int x = 0; x != layer.width (); ++x) {
        auto itr = segments_map.find(layer.tile_gid(x, y));
        // tile has no segments? skip
        if (itr == segments_map.end()) continue;
        // already loaded? skip
        if (rv.find(layer.tile_gid(x, y)) != rv.end()) continue;

        auto & classes = rv[layer.tile_gid(x, y)];
        auto segment_count = itr->second.segments.size();
        classes.reserve(segment_count);

        assert(layer(x, y) != nullptr);
        auto & props = *layer(x, y);
        auto decor_itr = props.find("decor-class");
        if (decor_itr == props.end()) {
            // has no decor-class... default all to "not ground"
            classes.resize(segment_count, false);
            continue;
        }
        for_split<is_comma>(decor_itr->second, [&classes](StrCItr beg, StrCItr end) {
            trim<is_whitespace>(beg, end);
            classes.push_back(std::equal(beg, end, "ground"));
        });
        if (classes.size() == segment_count) {
            // all ok
        } else if (classes.size() == 1) {
            // still ok, rest default to the same value as the first
            classes.resize(classes.front(), segment_count);
        } else {
            // not ok
            std::cout << "Class count mismatch with segment count. (There are "
                      << classes.size() << " decor classes, and "
                      << segment_count << " segments.)" << std::endl;
            classes.clear();
            classes.resize(segment_count, false);
        }
    }}
    return rv;
}

// multithreaded version
static std::unique_ptr<ForestDecor::FutureTreeMaker> make_future_tree_maker() {
    using FutureTreeMaker = ForestDecor::FutureTreeMaker;
    using FutureTree      = ForestDecor::FutureTree;
    class CompleteFutureTree final : public FutureTree {
    public:
        explicit CompleteFutureTree(std::future<PlantTree> && fut):
            m_future(std::move(fut)) {}
    private:
        bool is_ready() const override {
            return    m_future.wait_for(std::chrono::microseconds(0))
                   == std::future_status::ready;
        }

        bool is_done() const override { return m_done; }

        PlantTree get_tree() override {
            m_done = true;
            return m_future.get();
        }

        std::future<PlantTree> m_future;
        bool m_done = false;
    };

    struct CompleteTreeMaker final : public FutureTreeMaker {
        // even Alex Stepanov appearently was like: "ye my bad" with std::vector's name
        using TreePromise = std::promise<PlantTree>;
        using TaskList    = std::vector<std::pair<TreePromise, TreeParameters>>;

        CompleteTreeMaker() {
            m_thread = std::thread(&CompleteTreeMaker::worker_entry_point, this);
        }

        ~CompleteTreeMaker() override {
            m_worker_done = true;
            {
            std::unique_lock lk(m_mutex);
            // break those promises, erase those dreams, it's all over
            // death is upon us
            ms_tasks.clear();
            }
            m_hold_loop.notify_one();
            m_thread.join();
        }

        std::unique_ptr<FutureTree> make_tree
            (const TreeParameters & params) override
        {
            std::unique_ptr<FutureTree> rv;
            {
            std::unique_lock lk(m_mutex);
            ms_tasks.emplace_back(TreePromise(), params);
            rv = std::make_unique<CompleteFutureTree>(ms_tasks.back().first.get_future());
            }
            m_hold_loop.notify_one();
            return rv;
        }

        void worker_entry_point() {
            TaskList task_list;
            while (!m_worker_done) {
                // wait for more is task list is empty
                if (task_list.empty()) {
                    std::unique_lock lk(m_mutex);
                    m_hold_loop.wait(lk);
                }
                for (auto & [prom, params] : task_list) {
                    PlantTree tree;
                    tree.plant(params.location, static_cast<PlantTree::CreationParams>(params));
                    prom.set_value(std::move(tree));
                }
                task_list.clear();
                {
                std::unique_lock lk(m_mutex);
                task_list.swap(ms_tasks);
                }
            }
        }

        std::condition_variable m_hold_loop;

        std::thread m_thread;
        std::mutex m_mutex;
        std::atomic_bool m_worker_done { false };
        TaskList ms_tasks;
    };
    return std::make_unique<CompleteTreeMaker>();
}

// single thread version
static std::unique_ptr<ForestDecor::FutureTreeMaker> make_syncro_tree_maker() {
    using FutureTreeMaker = ForestDecor::FutureTreeMaker;
    using FutureTree      = ForestDecor::FutureTree;
    class CompleteFutureTree final : public FutureTree {
    public:
        CompleteFutureTree() {}
        void plant(VectorD location, const PlantTree::CreationParams & params)
            { m_tree.plant(location, params); }

    private:
        bool is_ready() const override { return true; }

        bool is_done() const override { return m_done; }

        PlantTree get_tree() override {
            m_done = true;
            return m_tree;
        }

        PlantTree m_tree;
        bool m_done = false;
    };

    struct CompleteTreeMaker final : public FutureTreeMaker {
        std::unique_ptr<FutureTree> make_tree
            (const TreeParameters & params) override
        {
            auto rv = std::make_unique<CompleteFutureTree>();
            rv->plant(params.location, static_cast<PlantTree::CreationParams>(params));
            return rv;
        }
    };
    return std::make_unique<CompleteTreeMaker>();
}

static std::shared_ptr<WfFramesInfo> load_new_strip
    (const tmap::TiledMap & tmap, int gid, const std::string & value_string)
{
    std::vector<WfStripInfo> strips;

    auto tileset = tmap.get_tile_set_for_gid(gid);
    auto verify_valid_local_id = [&tileset](int id) {
        if (tileset->convert_to_local_id(id) != tmap::TileSetInterface::k_no_tile) {
            return;
        }
        throw std::runtime_error("id " + std::to_string(id) + " not owned by this tileset.");
    };
    for_split<is_comma>(value_string.begin(), value_string.end(), [&verify_valid_local_id, &strips, gid](StrCItr beg, StrCItr end) {
        int id = 0;
        trim<is_whitespace>(beg, end);
        if (!string_to_number_multibase(beg, end, id)) {
            throw std::runtime_error("gid " + std::to_string(gid) + " starting strip frame is non-numeric.");
        }
        WfStripInfo new_strip;
        // all should be local ids!
        new_strip.min = id;
        new_strip.max = new_strip.min;
        new_strip.step = 1;
        new_strip.reset_id = new_strip.min;
        verify_valid_local_id(id);

        strips.push_back(new_strip);
    });

    for (auto & strip : strips) {
        const auto * props = tileset->properties_on(strip.min);
        if (!props) continue;
        auto do_if_found = make_do_if_found(*props);
        auto require_int = make_optional_requires_numeric<int>([](const std::string & key, const std::string &) {
            return std::runtime_error("key \"" + key + "\" must be numeric");
        });

        do_if_found("fall-max-id", require_int, [&strip, &verify_valid_local_id](int val) {
            verify_valid_local_id(val);
            if (val < strip.min) throw std::runtime_error("smaller than min not supported");
            strip.max = val;
        });
        do_if_found("fall-step", require_int, [&strip](int val) {
            if (val < 1) throw std::runtime_error("min step of one");
            strip.step = val;
        });
        do_if_found("fall-reset", require_int, [&strip, &verify_valid_local_id](int val) {
            verify_valid_local_id(val);
            if (val < strip.min || val > strip.max) {
                throw std::runtime_error("reset id must be in [min max]");
            }
            strip.reset_id = val;
        });

    }

    auto rv = std::make_shared<WfFramesInfo>();
    rv->setup(strips, tileset);
    return rv;
}
