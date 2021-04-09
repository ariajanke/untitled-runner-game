/****************************************************************************

    Copyright 2020 Aria Janke

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

#include "GraphicsDrawer.hpp"
#include "maps/LineMapLoader.hpp"

#include "FillIterate.hpp"
#include "maps/MapObjectLoader.hpp"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/RenderTexture.hpp>

#include <common/SubGrid.hpp>

#include <tmap/TiledMap.hpp>

#include <iostream>
#include <variant>
#include <unordered_set>
#include <thread>
#include <future>

#include <cassert>

namespace {

static const VectorD k_unit_start(1., 0.);

VectorD add_polar(VectorD r, double angle, double distance) {
    return r + rotate_vector(k_unit_start, angle)*distance;
}

sf::Vertex make_circle_vertex(double t)
    { return sf::Vertex(sf::Vector2f(float(std::cos(t)), float(std::sin(t)))); }

View<const sf::Vertex *> get_circle_verticies_for_radius(double rad);

} // end of <anonymous> namespace

void LineDrawer2::post_line(VectorD a, VectorD b, sf::Color color, double thickness) {
    auto init_angle = angle_between(a - b, k_unit_start);

    auto mk_vertex = [thickness, init_angle, color](VectorD pt, double ang_dir) {
        auto theta = init_angle + ang_dir*(k_pi / 2.0);
        auto r = add_polar(pt, theta, thickness / 2.0);
        return sf::Vertex(sf::Vector2f(r), color);
    };

    auto verticies = {
        mk_vertex(a,  1.0), mk_vertex(a, -1.0),
        mk_vertex(b, -1.0), mk_vertex(b,  1.0)
    };
    m_verticies.insert(m_verticies.end(), verticies.begin(), verticies.end());
}

void LineDrawer2::render_to(sf::RenderTarget & target) {
    static constexpr const auto k_quads = sf::PrimitiveType::Quads;
    assert(m_verticies.size() % 4 == 0);
    target.draw(m_verticies.data(), m_verticies.size(), k_quads);
    m_verticies.clear();
}

// ----------------------------------------------------------------------------

void CircleDrawer2::post_circle(VectorD r, double radius, sf::Color color) {
    auto old_size_ = m_vertices.size();
    auto get_begin = [this, old_size_]() { return m_vertices.begin() + old_size_; };
    auto vertex_range = get_circle_verticies_for_radius(radius);
    m_vertices.insert(m_vertices.end(), vertex_range.begin(), vertex_range.end());
    for (auto itr = get_begin(); itr != m_vertices.end(); ++itr) {
        itr->color = color;
        itr->position = float(radius)*itr->position + sf::Vector2f(r);
    }
}

void CircleDrawer2::render_to(sf::RenderTarget & target) {
    assert(m_vertices.size() % 3 == 0);
    target.draw(m_vertices.data(), m_vertices.size(), sf::PrimitiveType::Triangles);
    m_vertices.clear();
}

// ----------------------------------------------------------------------------

void FlagRaiser::post_flag_raise(ecs::EntityRef ref, VectorD bottom, VectorD top) {
    auto & rec = m_flag_records[ref];
    rec.start = bottom;
    rec.end = top;
    rec.time_passed = 0.;
}

void FlagRaiser::render_to(sf::RenderTarget & target) {
    for (const auto & [ref, rec] : m_flag_records) {
        (void)ref;
        target.draw(rec.draw_rect);
    }
}

void FlagRaiser::update(double et) {
    for (auto itr = m_flag_records.begin(); itr != m_flag_records.end(); ) {
        if (itr->first.has_expired()) {
            itr = m_flag_records.erase(itr);
        } else {
            auto & rec = itr->second;
            rec.time_passed += et;

            auto mag_ = magnitude(rec.end - rec.start);
            auto dir_ = normalize(rec.end - rec.start);
            auto offset = dir_*rec.time_passed*Record::k_raise_speed;
            if (magnitude(offset) > mag_) {
                offset = normalize(offset)*mag_;
            }
            float height = float(magnitude(offset));
            if (height > Record::k_height) height = Record::k_height;
            sf::Vector2f loc(rec.start + offset);
            rec.draw_rect = DrawRectangle(loc.x, loc.y, float(Record::k_width), height, sf::Color(50, 100, 200));

            ++itr;
        }
    }
}

// ----------------------------------------------------------------------------

void ItemCollectAnimations::post_effect(VectorD r, AnimationPtr aptr) {
    Record rec;
    rec.ptr = aptr;
    rec.current_frame = aptr->tile_ids.begin();
    rec.location = r;
    m_records.push_back(rec);
}

void ItemCollectAnimations::update(double et) {
    for (auto & rec : m_records) {
        if ((rec.elapsed_time += et) <= rec.ptr->time_per_frame) continue;
        assert(rec.current_frame != rec.ptr->tile_ids.end());
        rec.elapsed_time = 0.;
        ++rec.current_frame;
    }
    auto end = m_records.end();
    m_records.erase(std::remove_if(m_records.begin(), end, should_delete), end);
}

void ItemCollectAnimations::render_to(sf::RenderTarget & target) const {
    for (const auto & rec : m_records) {
        sf::Sprite brush;
        brush.setPosition(sf::Vector2f(rec.location));
        brush.setTexture(rec.ptr->tileset->texture());
        brush.setTextureRect(rec.ptr->tileset->texture_rectangle(*rec.current_frame));
        target.draw(brush);
    }
}

// ----------------------------------------------------------------------------

void Flower::setup(std::default_random_engine & rng) {
    using RealDistri = std::uniform_real_distribution<double>;
    auto mk_random_time = [&rng](double max) {
        return RealDistri(0., max)(rng);
    };

    static constexpr const double k_max_pistil_to_pop_time = 0.33;
    static constexpr const double k_max_at_pistil_pop_time = 8.;
    static constexpr const double k_max_petal_to_pop_time  = 0.33;
    static constexpr const double k_max_at_petal_pop_time  = 8.;

    m_to_pistil_pop      = mk_random_time(k_max_pistil_to_pop_time);
    m_time_at_pistil_pop = mk_random_time(k_max_at_pistil_pop_time);
    m_to_petal_pop       = mk_random_time(k_max_petal_to_pop_time );
    m_time_at_petal_pop  = mk_random_time(k_max_at_petal_pop_time );

    static constexpr const double k_min_stem_width = 2.;
    static constexpr const double k_max_stem_width = 4.;

    static constexpr const double k_min_stem_height =  8.;
    static constexpr const double k_max_stem_height = 20.;

    // have origin be the
    double w = RealDistri(k_min_stem_width, k_max_stem_width)(rng);
    m_stem = DrawRectangle(0.f, float(w / 2.), float(w),
         float(RealDistri(k_min_stem_height, k_max_stem_height)(rng)),
         random_stem_color(rng));

    m_pistil = DrawRectangle(0.f, 0.f, w, w, random_pistil_color(rng));

    static constexpr const double k_min_petal_delta = 2.;
    static constexpr const double k_max_petal_delta = 6.;
    {
    double petal_w = RealDistri(k_min_petal_delta, k_max_petal_delta)(rng) + w;
    double petal_h = RealDistri(k_min_petal_delta, k_max_petal_delta)(rng) + w;
    if (petal_w < petal_h)
        std::swap(petal_w, petal_h);
    m_petals = DrawRectangle(0.f, 0.f, float(petal_w), float(petal_h), random_petal_color(rng));
    }

    static constexpr const double k_min_pop_delta = 3.;
    static constexpr const double k_max_pop_delta = 6.;
    m_popped_position = RealDistri(k_min_pop_delta, k_max_pop_delta)(rng);
    pop_pistil(0.);
    pop_petal (0.);

    m_stem.set_x(-m_stem.width()*0.5f);
    m_pistil.set_x(-m_pistil.width()*0.5f);

}

void Flower::update(double et) {
    m_time += et;
    if (m_time > resettle_thershold()) {
        // begin resettling
        // if we resettle all the way, restart (m_time = 0.)
        auto amt = std::min(1., 3.*(m_time - resettle_thershold()) / resettle_thershold());
        pop_pistil(1. - amt);
        pop_petal (1. - amt);
        if (are_very_close(1., amt)) {
            m_time = 0.;
        }
    } else if (m_time > petal_thershold()) {
        // begin popping petal
        pop_pistil(1.);
        pop_petal (std::min(1., (m_time - petal_thershold()) / petal_thershold()));
    } else if (m_time > m_to_pistil_pop) {
        // begin popping pistil
        pop_pistil(std::min(1., (m_time - m_to_pistil_pop) / m_to_pistil_pop));
    }
}

/* private */ void Flower::draw(sf::RenderTarget & target, sf::RenderStates states) const {
    states.transform.translate(sf::Vector2f(m_location));
    target.draw(m_stem  , states);
    target.draw(m_petals, states);
    target.draw(m_pistil, states);
}


/* private */ double Flower::petal_thershold() const noexcept {
    return m_to_pistil_pop + m_time_at_pistil_pop;
}

/* private */ double Flower::resettle_thershold() const noexcept {
    return petal_thershold() + m_to_petal_pop + m_time_at_petal_pop;
}

/* private */ double Flower::pistil_pop_time() const noexcept {
    if (m_to_pistil_pop == k_inf) return 0.;
    return m_time - m_to_pistil_pop;
}

/* private */ void Flower::pop_pistil(double amount) {
    verify_0_1_interval("pop_pistil", amount);

    m_pistil.set_y(-m_petals.height()*0.5f - float(amount*m_popped_position));
}

/* private */ void Flower::pop_petal(double amount) {
    verify_0_1_interval("pop_petal", amount);
    m_petals.set_position(-m_petals.width()*0.5f,
                          -m_petals.height()*0.5f
                          - float(amount*m_popped_position));
}

/* private static */ void Flower::verify_0_1_interval(const char * caller, double amt) {
    if (amt >= 0. || amt <= 1.) return;
    throw std::invalid_argument(std::string(caller) + ": amount must be in [0 1].");
}

/* private static */ sf::Color Flower::random_petal_color
    (std::default_random_engine & rng)
{
    // avoid greens
    auto high  = UInt8Distri(200, k_max_u8 )(rng);
    auto low   = UInt8Distri( 20, high - 40)(rng);
    auto green = UInt8Distri(  0, high - 40)(rng);

    auto red  = high;
    auto blue = low;
    if (UInt8Distri(0, 1)(rng))
        std::swap(red, blue);
    return sf::Color(red, green, blue);
}

/* private static */ sf::Color Flower::random_pistil_color
    (std::default_random_engine & rng)
{
    auto yellow = UInt8Distri(160, k_max_u8)(rng);
    auto blue   = UInt8Distri(0, yellow / 3)(rng);
    return sf::Color(yellow, yellow, blue);
}

/* private static */ sf::Color Flower::random_stem_color
    (std::default_random_engine & rng)
{
    auto green = UInt8Distri(100, k_max_u8)(rng);
    auto others = UInt8Distri(0, green / 3)(rng);
    return sf::Color(others, green, others);
}

// ----------------------------------------------------------------------------

using GroundsClassMap = std::unordered_map<int, std::vector<bool>>;

static GroundsClassMap load_grounds_map
    (const tmap::TilePropertiesInterface & layer, const LineMapLoader::SegmentMap & segments_map);

// multithreaded version
static std::unique_ptr<ForestDecor::FutureTreeMaker> make_future_tree_maker();

// single thread version
static std::unique_ptr<ForestDecor::FutureTreeMaker> make_syncro_tree_maker();

std::string to_padded_string(int x) {
    if (x == 0) return "000";
    std::string rv = std::to_string(x);
    return std::string(std::size_t(2 - std::floor(std::log10(double(x)))), '0') + rv;
}

ForestDecor::~ForestDecor() {
    int i = 0;
    for (const auto & tree : m_trees) {
        tree.save_to_file("/media/ramdisk/tree-" + std::to_string(i++) + ".png");
    }
}
#if 0
void ForestDecor::load_map(const tmap::TiledMap & tmap, MapObjectLoader & objloader) {
    load_map_vegetation(tmap, objloader);
    load_map_waterfalls(tmap);
#   if 0
    for (std::size_t i = 0; i != m_trees.size(); ++i) {
        auto fn = "/media/ramdisk/generated-tree-" + to_padded_string(i) + ".png";
        m_trees[i].save_to_file(fn);
    }
#   endif
}
#endif
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

struct WfStripInfo {
    static constexpr const int k_uninit = -1;
    int step = k_uninit;
    int reset_id = k_uninit;
    int min = k_uninit;
    int max = k_uninit;
};

class WfFramesInfo final : public ForestDecor::Updatable {
public:
    using ConstTileSetPtr = tmap::TiledMap::ConstTileSetPtr;
    using TileEffect = tmap::TileEffect;

    WfFramesInfo() {}

    void setup(const std::vector<WfStripInfo> & strips, ConstTileSetPtr tsptr) {
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

    int get_new_tid(int num_of_same_above) const {
        assert(num_of_same_above > -1);
        auto seqnum = std::size_t(num_of_same_above);
        if (seqnum >= m_effects.size()) {
            seqnum = (seqnum % (m_effects.size() - m_reset_y)) + m_reset_y;
            assert(seqnum < m_effects.size());
        }
        return m_effect_tids.at(seqnum);
    }

    template <typename Func>
    void for_each_tid_and_tile_effect(Func && f) {
        for (auto & wfte : m_effects) {
            TileEffect & te = wfte;
            int gid = m_effect_tids[&wfte - &m_effects.front()];
            f(gid, te);
        }
    }

    void update(double et) override {
        m_time = std::fmod(m_time + et, 1.);
        check_invarients();
    }

private:
    class WfEffect final : public TileEffect {
    public:
        void assign_time(const double & time) { m_time_ptr = &time; }

        void setup_frames(const std::vector<int> & local_ids, ConstTileSetPtr tsptr) {
            m_frames.reserve(local_ids.size());
            for (int id : local_ids) {
                m_frames.push_back( tsptr->texture_rectangle(id) );
            }
        }

    private:
        using DrawOnlyTarget = tmap::DrawOnlyTarget;

        void operator () (sf::Sprite & spt, DrawOnlyTarget & target) override {
            assert(m_time_ptr);
            assert( *m_time_ptr >= 0. && *m_time_ptr <= 1. );
            auto idx = std::size_t( std::floor(double( m_frames.size() )*(*m_time_ptr)) );
            spt.setTextureRect( m_frames[idx] );
            target.draw(spt);
        }

        std::vector<sf::IntRect> m_frames;
        const double * m_time_ptr = nullptr;
    };

    void check_invarients() const {
        assert(m_reset_y <= m_effects.size());
        assert(m_time >= 0. && m_time <= 1.);
        assert(m_effects.size() == m_effect_tids.size());
    }

    double m_time = 0.;
    std::size_t m_reset_y = 0;
    std::vector<WfEffect> m_effects;
    std::vector<int> m_effect_tids;
};

using StrCItr = std::string::const_iterator;
static bool is_comma(char c) { return c == ','; }

struct ForestLoadTemp final : public MapDecorDrawer::TempRes {
    // THIS is fine actually
    std::map<int, std::shared_ptr<WfFramesInfo>> gid_to_strips;
};

std::shared_ptr<WfFramesInfo> load_new_strip
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
    m_tree_maker = make_future_tree_maker();

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

// ----------------------------------------------------------------------------

using Tag = Spine::Tag;
using Anchor = Spine::Anchor;

std::tuple<VectorD, VectorD> Tag::left_points() const {
    return points(-m_width*0.5);
}

std::tuple<VectorD, VectorD> Tag::right_points() const {
    return points(m_width*0.5);
}

Tag & Tag::set_location(VectorD r) {
    assert(is_real(r.x) && is_real(r.y));
    m_location = r;
    return *this;
}

Tag & Tag::set_direction(VectorD r) {
    assert(is_real(r.x) && is_real(r.y));
    assert(!are_very_close(r, VectorD()));
    assert(are_very_close(magnitude(r), 1.));
    m_direction = to_direction(r);
    return *this;
}

Tag & Tag::set_width_angle(double x) {
    assert(is_real(x));
    assert(x >= 0.);
    m_width = x;
    return *this;
}

VectorD Tag::location() const {
    return m_location;
}

VectorD Tag::direction() const {
    return to_unit_circle_vector(m_direction);
}

/* private */ std::tuple<VectorD, VectorD> Tag::points(double from_center) const {
    return std::make_tuple(
        m_location + 20.*to_unit_circle_vector(m_direction - from_center),
        m_location);
}

Anchor & Anchor::set_location(VectorD r) {
    assert(is_real(r.x) && is_real(r.y));
    m_location = r;
    return *this;
}

Anchor & Anchor::set_berth(double) {
    return *this;
}

Anchor & Anchor::set_width(double w) {
    assert(is_real(w));
    m_width = w;
    return *this;
}

Anchor & Anchor::set_pinch(double x) {
    assert(is_real(x));
    assert(x >= 0. && x <= 1.);
    m_pinch = x;
    return *this;
}

Anchor & Anchor::set_length(double l) {
    assert(is_real(l));
    m_length = l;
    return *this;
}

Anchor & Anchor::set_direction(VectorD r) {
    assert(is_real(r.x) && is_real(r.y));
    assert(!are_very_close(r, VectorD()));
    assert(are_very_close(magnitude(r), 1.));
    m_direction = to_direction(r);
    return *this;
}

void Anchor::sway_toward(const Tag &) {

}

VectorD Anchor::location() const {
    return m_location;
}

VectorD Anchor::direction() const {
    return to_unit_circle_vector(m_direction);
}

/* private */ std::tuple<VectorD, VectorD> Anchor::get_points(double offset) const {
    auto offsetv = to_unit_circle_vector(m_direction - k_pi*0.5)*offset;
    auto to_b = rotate_vector(to_unit_circle_vector(m_direction)*m_length,
                              normalize(offset)*k_pi*(1. / 6.));
    return std::make_tuple(m_location + offsetv,
                           m_location + offsetv*m_pinch + to_b);
}

// ----------------------------------------------------------------------------

bool contains_point(const std::vector<VectorI> & points, VectorI center, VectorI pt) {
    bool rv = false;
    auto in_stripe = [&] (VectorI a, VectorI b) {
        auto gv = find_intersection(VectorD(a), VectorD(b), VectorD(center), VectorD(pt));
        if (gv == k_no_intersection) {
            return fc_signal::k_continue;
        }
        rv = true;
        return fc_signal::k_break;
    };
    for_side_by_side(points, in_stripe);
    if (rv) return rv;
    if (!points.empty()) {
        (void)in_stripe(points.back(), points.front());
    }
    return rv;
}
bool not_contains_point(const std::vector<VectorI> & points, VectorI center, VectorI pt) {
    return !contains_point(points, center, pt);
}

template <typename Func>
void make_bundle_locations
    (unsigned seed, int count, int radius, int width, int height,
     Func && f
     )
{
    std::default_random_engine rng { seed };
    for_ellip_distri(
        Rect(radius, radius, width - radius*2, height - radius*2),
        count, rng, [&f](VectorD r)
    { f(round_to<int>(r)); });
}
#if 0
static bool is_comma(char c) { return c == ','; }
#endif
static GroundsClassMap load_grounds_map
    (const tmap::TilePropertiesInterface & layer,
     const LineMapLoader::SegmentMap & segments_map)
{
    using CStrIter = std::string::const_iterator;
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
        for_split<is_comma>(decor_itr->second, [&classes](CStrIter beg, CStrIter end) {
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

// ----------------------------------------------------------------------------

void GraphicsDrawer::render_front(sf::RenderTarget & target) {
    m_map_decor->render_front(target);
}

void GraphicsDrawer::render_back(sf::RenderTarget & target) {
    m_map_decor->render_back(target);
    m_line_drawer.render_to(target);
    m_circle_drawer.render_to(target);
    m_flag_raiser.render_to(target);
    for (const auto & spt : m_sprites) {
        target.draw(spt);
    }
    for (const auto & rect : m_draw_rectangles) {
        target.draw(rect);
    }
    m_item_anis.render_to(target);
    // clear once-per-frames
    m_sprites.clear();
    m_draw_rectangles.clear();
}

void GraphicsDrawer::set_view(const sf::View & view) {
    m_view_rect = Rect(VectorD( view.getCenter() - view.getSize()*0.5f), VectorD(view.getSize()));
}

/* private */ void GraphicsDrawer::draw_sprite(const sf::Sprite & spt) {
    Rect spt_rect;
    spt_rect.left = spt.getPosition().x;
    spt_rect.top  = spt.getPosition().y;
    spt_rect.width = spt.getTextureRect().width;
    spt_rect.height = spt.getTextureRect().height;
    if (!spt_rect.intersects(m_view_rect)) return;
    m_sprites.push_back(spt);
}

namespace {

template <typename T>
const T * as_cptr(const T & obj) { return &obj; }

View<const sf::Vertex *> get_circle_verticies_for_radius(double rad) {
    if (rad <= 0.) {
        throw std::invalid_argument("get_circle_verticies_for_radius: radius must be a positive real number.");
    }
    using std::make_tuple;
    static const auto k_pcount_list = {
        make_tuple(-k_inf,  6),
        make_tuple(   10.,  9),
        make_tuple(   15., 12),
        make_tuple(   20., 15),
        make_tuple(   50., 18),
        make_tuple(  100., 18),
        make_tuple(  150., 24)
    };

    static const auto init_stuff = []() {
        static std::vector<sf::Vertex> tris;
        static std::vector<std::size_t> indicies;
        if (!tris.empty())
            { return std::make_tuple(as_cptr(tris), as_cptr(indicies)); }
        indicies.reserve(k_pcount_list.size() + 1);
        indicies.push_back(0);
        auto add = [](double t1, double t2) {
             tris.push_back(sf::Vertex());
             tris.push_back(make_circle_vertex(t1));
             tris.push_back(make_circle_vertex(t2));
        };
        for (auto [t_, steps] : k_pcount_list) {
            (void)t_;
            double cstep = 2.*k_pi / double(steps);
            for (double t = 0.; t < 2.*k_pi; t += cstep) {
                add(t, std::min(t + cstep, 2.*k_pi));
            }
            indicies.push_back(tris.size());
        }
        return std::make_tuple(as_cptr(tris), as_cptr(indicies));
    };

    static const std::vector<sf::Vertex> & k_circle_vec = *std::get<0>(init_stuff());
    static const std::vector<std::size_t> & k_indicies = *std::get<1>(init_stuff());

    auto itr = std::lower_bound(
        k_pcount_list.begin(), k_pcount_list.end(), rad,
        [](const std::tuple<double, int> & t, double rad)
    { return std::get<0>(t) < rad; });
    assert(itr != k_pcount_list.end());
    auto beg_idx = k_indicies[itr - k_pcount_list.begin()];
    auto end_idx = k_indicies[(itr - k_pcount_list.begin()) + 1];
    return View<const sf::Vertex *>
        (&k_circle_vec.front() + beg_idx, &k_circle_vec.front() + end_idx);
}

} // end of <anonymous> namespace
