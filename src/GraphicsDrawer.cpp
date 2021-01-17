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

static std::unique_ptr<MapDecorDrawer::FutureTreeMaker> make_future_tree_maker();

std::string to_padded_string(int x) {
    if (x == 0) return "000";
    std::string rv = std::to_string(x);
    return std::string(std::size_t(2 - std::floor(std::log10(double(x)))), '0') + rv;
}

void MapDecorDrawer::load_map(const tmap::TiledMap & tmap) {
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
#       if 0
        target_seg.a = VectorD(target_seg.a.x*tile_size.width, target_seg.a.y*tile_size.height);
        target_seg.b = VectorD(target_seg.b.x*tile_size.width, target_seg.b.y*tile_size.height);
#       endif
        auto plant_location = (target_seg.a + target_seg.b)*0.5;
        auto plflower = &MapDecorDrawer::plant_new_flower;
        auto pltree   = &MapDecorDrawer::plant_new_future_tree;// new_tree;
        auto fp = choose_random(rng, { plflower, plflower, plflower, plflower, plflower, plflower, pltree });
        std::invoke(fp, *this, rng, plant_location);
    }}
#   if 1
    for (std::size_t i = 0; i != m_trees.size(); ++i) {
        auto fn = "/media/ramdisk/generated-tree-" + to_padded_string(i) + ".png";
        m_trees[i].save_to_file(fn);
    }
#   endif
}

void MapDecorDrawer::render_to(sf::RenderTarget & target) const {
    render_back(target);
    render_front(target);
#   if 0
    Rect draw_bounds(VectorD(target.getView().getCenter() - target.getView().getSize()*0.5f),
                     VectorD(target.getView().getSize()));
    for (const auto & flower : m_flowers) {
        Rect flower_bounds(flower.location(), VectorD(flower.width(), flower.height()));
        if (!draw_bounds.intersects(flower_bounds)) continue;
        target.draw(flower);
    }
    for (const auto & tree : m_trees) {
        // note: no culling
        target.draw(tree);
    }
#   endif
}

void MapDecorDrawer::render_front(sf::RenderTarget & target) const {
    Rect draw_bounds(VectorD(target.getView().getCenter() - target.getView().getSize()*0.5f),
                     VectorD(target.getView().getSize()));
    for (const auto & tree : m_trees) {
        if (!draw_bounds.intersects(tree.bounding_box())) continue;
        tree.render_fronts(target, sf::RenderStates::Default);
    }
}

void MapDecorDrawer::render_back(sf::RenderTarget & target) const {
    Rect draw_bounds(VectorD(target.getView().getCenter() - target.getView().getSize()*0.5f),
                     VectorD(target.getView().getSize()));
    for (const auto & flower : m_flowers) {
        Rect flower_bounds(flower.location(), VectorD(flower.width(), flower.height()));
        if (!draw_bounds.intersects(flower_bounds)) continue;
        target.draw(flower);
    }
    for (const auto & tree : m_trees) {
        // note: no culling
        if (!draw_bounds.intersects(tree.bounding_box())) continue;
        tree.render_backs(target, sf::RenderStates::Default);
    }
}

void MapDecorDrawer::update(double et) {
    for (auto & flower : m_flowers) {
        flower.update(et);
    }
    for (auto & fut_tree_ptr : m_future_trees) {
        if (fut_tree_ptr->is_ready()) {
            m_trees.emplace_back(fut_tree_ptr->get_tree());
        }
    }
    {
    auto end = m_future_trees.end();
    static auto tree_is_done = [](const std::unique_ptr<FutureTree> & ftree)
        { return ftree->is_done(); };
    m_future_trees.erase(
        std::remove_if(m_future_trees.begin(), end, tree_is_done),
        end);
    }
}

/* private */ void MapDecorDrawer::plant_new_flower(std::default_random_engine & rng, VectorD plant_location) {
    Flower flower;
    flower.setup(rng);
    flower.set_location(plant_location - VectorD(flower.width()*0.5, flower.height()));
    m_flowers.emplace_back(flower);
}

/* private */ void MapDecorDrawer::plant_new_tree(std::default_random_engine & rng, VectorD plant_location) {
    PlantTree tree;
    tree.plant(plant_location, rng);
    m_trees.push_back(tree);
}

/* private */ void MapDecorDrawer::plant_new_future_tree
    (std::default_random_engine & rng, VectorD location)
{
    m_future_trees.emplace_back(m_tree_maker->make_tree(location, rng));
}

static std::unique_ptr<MapDecorDrawer::FutureTreeMaker> make_future_tree_maker() {
    using FutureTreeMaker = MapDecorDrawer::FutureTreeMaker;
    using FutureTree      = MapDecorDrawer::FutureTree;
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
        CompleteTreeMaker() {
            m_thread = std::thread(&CompleteTreeMaker::worker_entry_point, this);
        }

        ~CompleteTreeMaker() override {
            m_worker_done = true;
            {
            std::unique_lock lk(m_mutex);
            ms_params.clear();
            // break those promises, erase those dreams, it's all over
            // death is upon us
            ms_promises.clear();
            }
            m_hold_loop.notify_one();
            m_thread.join();
        }

        std::unique_ptr<FutureTree> make_tree
            (VectorD location, std::default_random_engine & rng) override
        {
            std::unique_ptr<FutureTree> rv;
            {
            std::unique_lock lk(m_mutex);
            ms_params.emplace_back(location, std::uniform_int_distribution<unsigned>()(rng));
            rv = std::make_unique<CompleteFutureTree>(ms_promises.emplace_back().get_future());
            }
            m_hold_loop.notify_one();
            return rv;
        }

        void worker_entry_point() {
            std::vector<std::pair<VectorD, unsigned>> task_list;
            std::vector<std::promise<PlantTree>> promises;
            while (!m_worker_done) {
                // wait for more is task list is empty
                if (task_list.empty()) {
                    std::unique_lock lk(m_mutex);
                    m_hold_loop.wait(lk);
                }

                assert(task_list.size() == promises.size());
                for (std::size_t i = 0; i != task_list.size(); ++i) {
                    auto & promise = promises[i];
                    auto & params  = task_list[i];
                    PlantTree tree;
                    std::default_random_engine rng { params.second };
                    tree.plant(params.first, rng);
                    promise.set_value(std::move(tree));
                }
                task_list.clear();
                promises.clear();

                {
                std::unique_lock lk(m_mutex);
                task_list.swap(ms_params);
                promises.swap(ms_promises);
                }
            }
        }

        std::condition_variable m_hold_loop;

        std::thread m_thread;
        std::mutex m_mutex;
        std::atomic_bool m_worker_done { false };

        std::vector<std::pair<VectorD, unsigned>> ms_params;
        std::vector<std::promise<PlantTree>> ms_promises;
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
#if 0
template <typename T>
class BezierDetails {
public:
    using VecT = sf::Vector2<T>;
    template <typename ... Types>
    using Tuple = std::tuple<Types...>;

    template <typename Func, typename ... Types>
    static void for_points(const Tuple<Types...> & tuple, T step, Func && f) {
        verify_step(step, "for_points");
        for (T v = 0; v < 1; v += step) {
            f(compute_point_tuple(v, tuple));
        }
        f(compute_point_tuple(1, tuple));
    }

    template <typename Func, typename ... Types>
    static void for_lines(const Tuple<Types...> & tuple, T step, Func && f) {
        verify_step(step, "for_lines");
        for (T v = 0; v < 1; v += step) {
            T next = std::min(T(1), v + step);
            f(compute_point_tuple(v, tuple), compute_point_tuple(next, tuple));
        }
    }

    template <typename ... Types>
    static VecT compute_point_tuple(T t, const Tuple<Types...> & tuple) {
        return compute_point_tuple<sizeof...(Types)>(t, tuple, std::index_sequence_for<Types...>());
    }

private:
    static void verify_step(T t, const char * caller) {
        if (t >= 0 && t <= 1) return;
        throw std::invalid_argument(std::string(caller)
                + ": step must be in [0 1].");
    }

    template <std::size_t k_tuple_size, typename TupleT, std::size_t ... kt_indicies>
    static VecT compute_point_tuple(T t, const TupleT & tuple, std::index_sequence<kt_indicies...>) {
        return compute_point<k_tuple_size>(t, std::get<kt_indicies>(tuple)...);
    }

    template <std::size_t k_tuple_size, typename ... Types>
    static VecT compute_point(T, Types ...)
        { return VecT(); }

    template <std::size_t k_tuple_size, typename ... Types>
    static VecT compute_point(T t, const VecT & r, Types ... args) {
        static_assert(k_tuple_size > sizeof...(Types), "");

        static constexpr const auto k_degree = k_tuple_size - 1;
        static constexpr const auto k_0p_degree = ( k_degree - sizeof...(Types) );
        static constexpr const auto k_1m_degree = k_degree - k_0p_degree;

        static constexpr const T k_scalar
            = ( k_0p_degree == k_degree || k_1m_degree == k_degree )
            ? T(1) : T(k_degree);

        return k_scalar*interpolate<k_1m_degree, k_0p_degree>(t)*r
               + compute_point<k_tuple_size>(t, std::forward<Types>(args)...);
    }

    template <std::size_t k_degree>
    static T interpolate_1m([[maybe_unused]] T t) {
        if constexpr (k_degree == 0)
            { return 1; }
        else
            { return (1 - t)*interpolate_1m<k_degree - 1>(t); }
    }

    template <std::size_t k_degree>
    static T interpolate_0p([[maybe_unused]] T t) {
        if constexpr (k_degree == 0)
            { return 1; }
        else
            { return t*interpolate_0p<k_degree - 1>(t); }
    }

    template <std::size_t k_m1_degree, std::size_t k_0p_degree>
    static T interpolate(T t) {
        return interpolate_1m<k_m1_degree>(t)*interpolate_0p<k_0p_degree>(t);
    }
};

template <typename T, typename Func, typename ... Types>
void for_bezier_points
    (const std::tuple<sf::Vector2<T>, Types...> & tuple, T step, Func && f)
{ return BezierDetails<T>::for_points(tuple, step, std::move(f)); }

template <typename T, typename Func, typename ... Types>
void for_bezier_lines
    (const std::tuple<sf::Vector2<T>, Types...> & tuple, T step, Func && f)
{ return BezierDetails<T>::for_lines(tuple, step, std::move(f)); }

template <typename T, typename ... Types>
sf::Vector2<T> compute_bezier_point
    (T t, const std::tuple<sf::Vector2<T>, Types...> & tuple)
{ return BezierDetails<T>::compute_point_tuple(t, tuple); }
#endif
#if 0
template <typename Func>
void plot_bresenham_line(VectorI a, VectorI b, Func && f) {
    // More information about this algorithm can be found here:
    // https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
    static auto norm = [](int x) { return x <= 0 ? -1 : 1; };
    auto dx  =  magnitude(b.x - a.x);
    auto sx  =  norm     (b.x - a.x);
    auto dy  = -magnitude(b.y - a.y);
    auto sy  =  norm     (b.y - a.y);
    auto err = dx + dy;
    while (true) {
        f(a);
        if (a == b) return;
        auto e2 = 2*err;
        if (e2 >= dy) {
            err += dy;
            a.x += sx;
        }
        if (e2 <= dx) {
            err += dx;
            a.y += sy;
        }
    }
}
#endif
#if 0
static sf::Image to_image(const ConstSubGrid<sf::Color> cgrid) {
    sf::Image img;
    img.create(unsigned(cgrid.width()), unsigned(cgrid.height()));
    for (VectorI r; r != cgrid.end_position(); r = cgrid.next(r)) {
        img.setPixel(unsigned(r.x), unsigned(r.y), cgrid(r));
    }
    return img;
}
#endif
#if 0
class SineDistribution {
public:
    SineDistribution(double min, double max): m_min(min), m_max(max) {}

    template <typename Urng>
    double operator () (Urng & rng) const {
        auto x = std::uniform_real_distribution<double>(k_error, 1. - k_error)(rng);
        return m_min + (m_max - m_min)*(std::acos(1 - x) / k_pi);
    }
private:
    double m_min, m_max;
};

template <typename Urng, typename Func>
void for_ellip_distri(const Rect & bounds, int times_done, Urng & rng, Func && f) {
    using RealDistri = std::uniform_real_distribution<double>;
    VectorD origin = center_of(bounds);
    //double ray_length = std::max(bounds.width, bounds.height)*1.1;
    auto hrad = bounds.width *0.5;
    auto vrad = bounds.height*0.5;
    for (int i = 0; i != times_done; ++i) {
        auto dir = to_unit_circle_vector(RealDistri(0, k_pi*2.)(rng));
        //auto intx = find_intersection(bounds, origin, origin + dir*ray_length);
        //auto gv = magnitude(find_intersection(bounds, origin, origin + dir*ray_length) - origin);
        auto r = hrad*vrad / std::sqrt(  vrad*vrad*dir.x*dir.x
                                       + hrad*hrad*dir.y*dir.y  );
        assert(is_real(r));
        auto mag = SineDistribution(-r, r)(rng);
        f(origin + dir*mag);
    }
}
#endif
//using VecVecIter = std::vector<VectorI>::const_iterator;
#if 0
template <typename T, typename IterType>
sf::Vector2<T> find_center(IterType beg, IterType end) {
    // testing for overflow will be a fun test case
    //static_assert(std::is_same_v<std::remove_const_t<std::remove_reference_t<decltype(*beg)>>, VectorI>, "");
    sf::Vector2<T> avg;
    for (auto itr = beg; itr != end; ++itr) {
        avg += *itr;
    }
    avg.x /= (end - beg);
    avg.y /= (end - beg);
    return avg;
}

template <typename Container>
[[deprecated]] VectorI find_center(const Container & points) {
    using IterType = decltype(std::begin(points));
    using ValueType = typename Container::value_type;
    auto rv = find_center<ValueType, IterType>(std::begin(points), std::end(points));
    if constexpr (std::is_floating_point_v<ValueType>) {
        return round_to<int>(rv);
    }
    return rv;
}
#endif
#if 0
template <typename Iter, typename Func>
void for_side_by_side_wrap(Iter beg, Iter end, Func && f) {
    if (end - beg < 2) return;
    for (auto itr = beg; itr != end - 1; ++itr) {
        const auto & a = *itr;
        const auto & b = *(itr + 1);
        auto fc = adapt_to_flow_control_signal(f, a, b);
        if (fc == fc_signal::k_break) return;
    }
    const auto & a = *(end - 1);
    const auto & b = *beg;
    f(a, b);
}
#endif
#if 0
template <typename IterType, typename T>
bool contains_point
    (IterType beg, IterType end,
     const sf::Vector2<T> & center, const sf::Vector2<T> & pt)
{
    using FVec = sf::Vector2<T>;
    bool rv = false;
    for_side_by_side_wrap(beg, end, [&] (FVec a, FVec b) {
        auto gv = find_intersection(VectorD(a), VectorD(b), VectorD(center), VectorD(pt));
        if (gv == k_no_intersection) {
            return fc_signal::k_continue;
        }
        rv = true;
        return fc_signal::k_break;
    });
    return rv;
}
#endif
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


#if 0
void insert_uncrossed(std::vector<VectorI> & cont, VectorI a) {
    // assumed: container is uncrossed
    if (cont.size() < 3) {
        cont.push_back(a);
        return;
    }
    static constexpr const std::size_t k_uninit = -1;
    std::size_t insert_point = k_uninit;
    for_side_by_side_wrap(cont.begin(), cont.end(),
        [a, &insert_point, &cont](const VectorI & b1, const VectorI & b2)
    {
        if (   (b1 == VectorI(1, 0) || b1 == VectorI(0, 1))
            && (b2 == VectorI(1, 0) || b2 == VectorI(0, 1)))
        {
            int j = 0;
            ++j;
        }
        bool crossing_found = false;
        for_side_by_side_wrap(cont.begin(), cont.end(), [&](const VectorI & c1, const VectorI & c2) {
            // this segment will end up being removed if this addition is
            // cleared
            if (&c1 == &b1 && &c2 == &b2) return fc_signal::k_continue;
            LineSegment c_seg{VectorD(c1), VectorD(c2)};
            VectorD gv1, gv2;
            if (b1 == c1 || b1 == c2) {
                gv1 = k_no_intersection;
            } else {
                gv1 = find_intersection(c_seg, VectorD(a), VectorD(b1));
            }
            if (b2 == c1 || b2 == c2) {
                gv2 = k_no_intersection;
            } else {
                gv2 = find_intersection(c_seg, VectorD(a), VectorD(b2));
            }
            if (gv1 == k_no_intersection && gv2 == k_no_intersection) {
                return fc_signal::k_continue;
            }
            crossing_found = true;
            return fc_signal::k_break;
        });
        if (!crossing_found) {
            insert_point = (&cont.back() - &b1) + 1;
            return fc_signal::k_break;
        }
        return fc_signal::k_continue;
    });

    if (insert_point == k_uninit) {
        throw "idk what to do.";
    }
    cont.insert(cont.begin() + insert_point, a);
};

std::vector<VectorI> generate_convex_hull(std::vector<VectorI> points) {
    if (points.size() < 4) return points;
    std::vector<VectorI> cand;
    cand.reserve(4);
    {
    // extremes
    using Lims = std::numeric_limits<int>;
    VectorI lowx(Lims::max(), 0), highx(Lims::min(), 0);
    VectorI lowy(0, Lims::max()), highy(0, Lims::min());
    for (const auto & r : points) {
        if (lowx.x  > r.x) lowx  = r;
        if (lowy.y  > r.y) lowy  = r;
        if (highx.x < r.x) highx = r;
        if (highy.y < r.y) highy = r;
    }
    std::array init_pts = { lowx, lowy, highx, highy };
    auto end = std::unique(init_pts.begin(), init_pts.end());
    cand.insert(cand.begin(), init_pts.begin(), end);
    }
    {
    auto c = find_center(cand);
    auto pt_not_cont = std::bind(not_contains_point, std::cref(cand), c, std::placeholders::_1);
    points.erase(std::remove_if(points.begin(), points.end(), pt_not_cont),
                 points.end());
    }

    // we only cull one point
    auto cull_cand = [] {
        std::vector<VectorI> cull_temp;
        return [cull_temp] (std::vector<VectorI> & cand) mutable {
            for (auto itr = cand.begin(); itr != cand.end(); ++itr) {
                cull_temp.clear();
                cull_temp.insert(cull_temp.begin(), cand.begin(), itr);
                cull_temp.insert(cull_temp.begin(), itr + 1, cand.end());
                auto temp_center = find_center(cull_temp);
                if (contains_point(cull_temp, temp_center, *itr)) {
                    // we can remove itr
                    cand.swap(cull_temp);
                    return;
                }
            }
        };
    } ();

    auto cand_center = find_center(cand);
    for (; !points.empty(); points.pop_back()) {
        insert_uncrossed(cand, points.back());
        // cull
        cull_cand(cand);
        cand_center = find_center(cand);
    }
    return cand;
}
#endif
#if 0
class GwPoints {
public:
    class Interface {
    public:
        using ConstVecPtr = const VectorI *;
        virtual ~Interface() {}
        virtual VectorI get_init_v() = 0;
        virtual void mark_done_with(ConstVecPtr &) = 0;
        virtual const std::vector<VectorI> & points() const = 0;
    };

    class WritablePts final : public Interface {
    public:
        WritablePts(const std::vector<VectorI> & copy_): m_copy(copy_) {}
    private:
        VectorI get_init_v() override {
            auto gv = get_extremes(m_copy);
            auto rv = gv.front();
            auto end = std::unique(gv.begin(), gv.end());

            auto c = find_center<int>(gv.begin(), end);

            auto pt_not_cont = [&gv, end, c](VectorI r) {
                return !contains_point(gv.begin(), end, c, r);
            };
            m_copy.erase(std::remove_if(m_copy.begin(), m_copy.end(), pt_not_cont),
                         m_copy.end());
            return rv;
        }

        void mark_done_with(ConstVecPtr & p) override {
            assert(!m_copy.empty());
            assert(p > &m_copy.front() && p < &m_copy.back());
            auto itr = (p - &m_copy.front()) + m_copy.begin();
            if (itr != m_copy.end())
                std::swap(*itr, *(m_copy.end() - 1));
            m_copy.pop_back();
            p = nullptr;
        }
        const std::vector<VectorI> & points() const override {
            return m_copy;
        }

        std::vector<VectorI> m_copy;
    };

    class ReadOnlyPts final : public Interface {
    public:
        ReadOnlyPts(const std::vector<VectorI> & cref): m_ref(cref) {}
    private:
        VectorI get_init_v() override
            { return get_extremes(m_ref).front(); }
        void mark_done_with(ConstVecPtr & p) override { p = nullptr; }
        const std::vector<VectorI> & points() const override { return m_ref; }
        const std::vector<VectorI> & m_ref;
    };
    using PtsVari = std::variant<WritablePts, ReadOnlyPts>;

    static PtsVari make_pt_handler(const std::vector<VectorI> & vec) {
        return (vec.size() > 1000) ? PtsVari { WritablePts(vec) } : PtsVari { ReadOnlyPts(vec) };
    }

    static Interface * get_interface(PtsVari & var) {
        struct Vis {
            Vis(Interface ** rv_): rv(rv_) {}
            void operator () (WritablePts & r) const { *rv = &r; }
            void operator () (ReadOnlyPts & r) const { *rv = &r; }
            Interface ** rv = nullptr;
        };
        Interface * rv = nullptr;
        std::visit(Vis(&rv), var);
        return rv;
    }

private:
    friend class WritablePts;
    friend class ReadOnlyPts;
    // lowx is always first element
    static std::array<VectorI, 4> get_extremes(const std::vector<VectorI> & vec) {
        using Lims = std::numeric_limits<int>;
        VectorI lowx(Lims::max(), 0), highx(Lims::min(), 0);
        VectorI lowy(0, Lims::max()), highy(0, Lims::min());
        for (const auto & r : vec) {
            if (lowx.x  > r.x) lowx  = r;
            if (lowy.y  > r.y) lowy  = r;
            if (highx.x < r.x) highx = r;
            if (highy.y < r.y) highy = r;
        }
        return { lowx, lowy, highx, highy };
    }
};
#endif
#if 0
// an implementation of the gift wrapping algorithm
std::vector<VectorI> find_convex_hull(const std::vector<VectorI> & pts);
std::vector<VectorI> find_convex_hull(GwPoints::Interface & pts_intf);

std::vector<VectorI> find_convex_hull(GwPoints::Interface & pts_intf) {
    if (pts_intf.points().size() < 3) return std::vector<VectorI>();

    // "current" vector being processed
    VectorI v = pts_intf.get_init_v();

    static const VectorI k_ex_left_start_dir(-1, 0);
    static const double  k_rot_dir = [] {
        static const VectorI k_dir_prev(0, 1);
        auto gv = rotate_vector(VectorD(k_dir_prev), k_pi*0.5);
        if (round_to<int>(gv) == k_ex_left_start_dir) return 1.;
        return -1.;
    } ();

    std::vector<VectorI> rv;
    rv.push_back(v);

    VectorD vn { k_ex_left_start_dir };
    do {
        VectorD cand_n = vn;
        const VectorI * cand_v = nullptr;
        double cand_rot = k_pi*2.;
        // need to handle colinears
        // so we'll favor the furthest point in our search
        double cand_dist = 0.;
        for (const auto & pt : pts_intf.points()) {
            // ignore other equal points, our candidate hull already "contains"
            // the point
            if (v == pt) continue;
            // definitely NOT the zero vector
            auto to_v = VectorD(v - pt);

            VectorD cons_n = normalize(rotate_vector(to_v, k_pi*0.5*k_rot_dir));
            double cons_dist = magnitude(VectorD(v - pt));
            auto cons_rot = angle_between(vn, cons_n);
            if (!are_very_close( rotate_vector(vn, cons_rot*k_rot_dir), cons_n))
                cons_rot = k_pi*2 - cons_rot;
            assert(are_very_close(rotate_vector(vn, cons_rot*k_rot_dir), cons_n));
            if (are_very_close(cons_rot, cand_rot)) {
                if (cons_dist <= cand_dist)
                    continue;
            } else if (cons_rot > cand_rot) {
                continue;
            }
            // new candidate
            cand_v    = &pt;
            cand_n    = cons_n;
            cand_rot  = cons_rot;
            cand_dist = cons_dist;
        }
        // post search
        // cand_v needs to not be v
        assert(cand_v);
        rv.push_back(v = *cand_v);
        assert(rv.size() <= pts_intf.points().size());
        pts_intf.mark_done_with(cand_v);
        vn = cand_n;
    } while (rv.front() != v);
    rv.pop_back();

    return rv;
}

std::vector<VectorI> find_convex_hull(const std::vector<VectorI> & pts) {
    auto varlivingspace = GwPoints::make_pt_handler(pts);
    return find_convex_hull(*GwPoints::get_interface(varlivingspace));
}
#endif
#if 0
template <bool k_is_const, typename T>
using SelectVecIterator =
    std::conditional_t<k_is_const, typename std::vector<T>::const_iterator, typename std::vector<T>::iterator>;

template <bool k_is_const, typename T>
using SelectStdVectorRef =
    std::conditional_t<k_is_const, const typename std::vector<T> &, typename std::vector<T> &>;
#endif
#if 0
// gets denormalized vector jutting out of the hull at a given segment
// normalizing this vector will get the normal of this hull triangle/segment
template <typename T>
sf::Vector2<T> get_hull_out_of_segment(const sf::Vector2<T> & a, const sf::Vector2<T> & b, const sf::Vector2<T> & hull_center) {
    VectorD mid { (a + b) / T(2) };
    VectorD from = mid - VectorD(hull_center);
    VectorD para = rotate_vector(VectorD(a - b), k_pi*0.5);
    if (angle_between( from, para ) >= angle_between( from, -para )) {
        para *= -1.;
    }
    return sf::Vector2<T>(para);
}
#endif
#if 0
template <bool k_is_const, typename T>
std::vector<SelectVecIterator<k_is_const, sf::Vector2<T>>>
    find_opposite_sides(SelectStdVectorRef<k_is_const, sf::Vector2<T>> cont,
                        const sf::Vector2<T> & hull_center)
{
    using Vec = sf::Vector2<T>;
    std::vector<SelectVecIterator<k_is_const, Vec>> rv;
    static constexpr const double k_init_ray_length = 1000.;
    rv.reserve(cont.size());

    for_side_by_side_wrap(cont.begin(), cont.end(), [&](const Vec & a, const Vec & b) {
        auto norm = normalize(get_hull_out_of_segment(VectorD(a), VectorD(b), VectorD(hull_center)));
        auto mid  = (VectorD(a) + VectorD(b))*0.5;
        auto ray_length = k_init_ray_length;

        const Vec * cand = nullptr;
        while (!cand) {
            if (!is_real(ray_length)) {
                throw std::invalid_argument("Cannot find opposite of a segment");
            }

            for_side_by_side_wrap(cont.begin(), cont.end(), [&](const Vec & ap, const Vec & bp) {
                if (&a == &ap) return fc_signal::k_continue;
                auto intx = find_intersection(mid, mid + -ray_length*norm, VectorD(ap), VectorD(bp));
                if (intx == k_no_intersection) return fc_signal::k_continue;
                cand = &ap;
                return fc_signal::k_break;
            });
            ray_length *= 2.;
        }
        rv.push_back( cont.begin() + (cand - &cont.front()) );
    });
    return rv;
}
#endif
#if 0
struct HashVector {
    std::size_t operator () (const VectorI & r) const {
        static auto as_u32 = [](const int & i) { return reinterpret_cast<const uint32_t &>(i); };
        return (std::size_t(as_u32(r.x)) << 32) ^ as_u32(r.y);
    }
};
#endif
#if 0
using BezierTriple = std::tuple<VectorD, VectorD, VectorD>;
using BezierTripleIter = std::vector<BezierTriple>::const_iterator;

static const auto k_no_location = k_no_intersection;
#endif
#if 0
template <typename OnHolesFunc, typename OnFillsFunc>
void for_each_holes_and_fills
    (BezierTripleIter beg, BezierTripleIter end,
     OnHolesFunc && on_holes, OnFillsFunc && on_fills)
{
    static auto is_hole = [](const BezierTriple & triple)
        { return std::get<1>(triple) == k_no_location; };
    static auto is_not_hole = [](const BezierTriple & triple)
        { return !is_hole(triple); };    
    auto handle_seq = [&on_fills, &on_holes]
        (bool seq_is_hole, BezierTripleIter beg, BezierTripleIter end, int idx)
    {
        // having an integer index seems like the least intrusive way to do it
        if (seq_is_hole) { assert(std::all_of(beg, end, is_hole    )); }
        else             { assert(std::all_of(beg, end, is_not_hole)); }
        if (seq_is_hole) { on_holes(beg, end, idx); }
        else             { on_fills(beg, end, idx); }
    };

    auto itr = beg;
    bool seq_is_hole = is_hole(*itr);
    int count_in_first = 0;

    for (auto jtr = itr; jtr != end; ++jtr) {
        if (is_hole(*jtr) != seq_is_hole) {
            if (count_in_first) {
                handle_seq(seq_is_hole, itr, jtr, itr - beg);
            }
            if (!count_in_first) count_in_first = (jtr - itr);
            seq_is_hole = is_hole(*jtr);
            itr = jtr;
        }
    }
    if (count_in_first == 0) {
        handle_seq(seq_is_hole, beg, end, 0);
    } else if (seq_is_hole == is_hole(*beg)) {
        auto needed = std::size_t((end - itr) + count_in_first);
        auto first_end = beg + count_in_first;
        std::vector<BezierTriple> temp;
        temp.reserve(needed);
        std::copy(beg, first_end, std::copy(itr, end, std::back_insert_iterator(temp)));
        handle_seq(seq_is_hole, temp.begin(), temp.end(), itr - beg);
    } else {
        handle_seq(seq_is_hole, itr, end, itr - beg);
        handle_seq(is_hole(*beg), beg, beg + count_in_first, 0);
    }
}
#endif
#if 0
namespace tree_bundle_classes {

enum Enum {
    k_air          =     0,
    k_unclassified =  0b01,
    k_island       =  0b10,
    k_body         =  0b11,
    // pop over to the third bit
    k_hull         = 0b100,
};

}

using TreeBundleClass = tree_bundle_classes::Enum;
#endif
#if 0
namespace tree_bundle_classes {

enum Enum {
    k_air          =     0,
    k_unclassified =  0b01,
    k_island       =  0b10,
    k_body         =  0b11,
};

}

class BundleClass {
public:
    BundleClass(): m_class(tree_bundle_classes::k_air), m_hull(false) {}

    bool is_air   () const noexcept { return m_class == tree_bundle_classes::k_air; }
    bool is_bundle() const noexcept { return m_class != tree_bundle_classes::k_air; }
    bool is_hull  () const noexcept { return m_hull; }

    void mark_as_hull() {
        if (!is_bundle()) throw std::runtime_error("");
        m_hull = true;
    }

    void classify_as(tree_bundle_classes::Enum clss) { m_class = clss; }
    tree_bundle_classes::Enum get_class() const noexcept
        { return tree_bundle_classes::Enum(m_class); }
private:
    //static constexpr const
    uint8_t m_class : 2;
    bool m_hull : 1;
};
#endif
#if 0
void classify_bundles(const std::vector<VectorI> & bundle_points, int radius, SubGrid<BundleClass>, VectorI body_root);

void classify_bundles
    (const std::vector<VectorI> & bundle_points, int radius,
     SubGrid<BundleClass> classgrid, VectorI body_root)
{
    for (const auto & pt : bundle_points) {
        auto cgrid = make_sub_grid(classgrid, pt - VectorI(1, 1)*radius, radius*2, radius*2);
        for (VectorI r; r != cgrid.end_position(); r = cgrid.next(r)) {
            if (r.x*r.x + r.y*r.y < radius*radius) continue;
            cgrid(r).classify_as(tree_bundle_classes::k_unclassified);
        }
    }
    iterate_grid_group(classgrid, body_root,
                       [&classgrid](VectorI r) { return !classgrid(r).is_air(); },
                       [&classgrid](VectorI r, bool) { classgrid(r).classify_as(tree_bundle_classes::k_body); });
    for (auto & bc : classgrid) {
        if (bc.get_class() == tree_bundle_classes::k_unclassified)
            bc.classify_as(tree_bundle_classes::k_island);
    }
}
#endif
#if 0
std::vector<TreeBundleClass> classify_tree_body_points(ConstSubGrid<bool> bundle_zones, const std::vector<VectorI> & bundle_locations);
#endif
#if 0
std::vector<VectorI> find_tree_bundle_islands(ConstSubGrid<bool> bundle_zones, const std::vector<VectorI> & bundle_locations);
#endif
#if 0
struct PlaceBundle {
    virtual void operator () (VectorI location) const = 0;
};
#endif
template <typename Func>
void make_bundle_locations
    (unsigned seed, int count, int radius, int width, int height,
#   if 0
     const PlaceBundle & place
#   endif
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
template <typename Func>
void make_bundle_locations(unsigned seed, int count, int radius, int width, int height,
                           std::enable_if_t<!std::is_base_of_v<PlaceBundle, Func>, Func> && f)
{
    struct Temp final : public PlaceBundle {
        Temp(Func && f_): f(f_) {}
        void operator () (VectorI r) const override { f(r); }
        Func f;
    };
    Temp temp(std::move(f));
    make_bundle_locations(seed, count, radius, width, height, temp);
}
#endif
#if 0
std::vector<BezierTriple> make_triples
    (const std::vector<VectorI> & hull_points, ConstSubGrid<BundleClass>, int radius);

std::vector<BezierTriple> make_triples
    (const std::vector<VectorI> & hull_points_vec, ConstSubGrid<BundleClass> grid_class,
     int radius)
{
    auto hull_center = find_center<int>(hull_points_vec.begin(), hull_points_vec.end());
    auto opposites = find_opposite_sides<true>(hull_points_vec, hull_center);
    // dips are the middle points
    std::vector<BezierTriple> bezier_triples;
    bezier_triples.reserve(hull_points_vec.size());
    for_side_by_side_wrap(hull_points_vec.begin(), hull_points_vec.end(),
        [&](const VectorI & a, const VectorI & b)
    {
        auto para = normalize(get_hull_out_of_segment(VectorD(a), VectorD(b), VectorD(hull_center)));
        auto mid = (VectorD(a) + VectorD(b))*0.5 + para;
        // step until we hit an occupied pixel or we cross the "opposite" side
        VectorD dip_pos = k_no_location;
        VectorD step;
        double stop_distance;
        {
        auto itr = *(opposites.begin() + (&a - &hull_points_vec.front()));
        VectorD stop_a { *itr };
        VectorD stop_b { itr + 1 == hull_points_vec.end() ? *hull_points_vec.begin() : *(itr + 1) };
        auto to_stop = ( (stop_a + stop_b)*0.5 ) - mid;
        assert( find_intersection(stop_a, stop_b, mid, mid + to_stop*1.1) != k_no_intersection );
        step = normalize(to_stop);
        stop_distance = magnitude(to_stop);
        }
        for (auto pos = mid + step; magnitude(pos - mid) < stop_distance; pos += step)
        {
            if (!grid_class(round_to<int>(pos)).is_air()) {
                dip_pos = pos;
                break;
            }
        }
        bezier_triples.emplace_back(
            VectorD(a) + para*double(radius),
            dip_pos,
            VectorD(b) + para*double(radius));
    });
    assert(bezier_triples.size() == hull_points_vec.size());
    return bezier_triples;
}
#endif
#if 0
VectorD find_hull_center_without_sunken(const std::vector<BezierTriple> &, const std::vector<VectorI> & hull_points_vec);

VectorD find_hull_center_without_sunken
    (const std::vector<BezierTriple> & bezier_triples, const std::vector<VectorI> & hull_points_vec)
{
    VectorD nhull_center = k_no_location;
    auto avg_taker = [] {
        int i = 0;
        return [i] (VectorD & avg, VectorD r) mutable {
            if (i++ == 0) {
                avg = r;
                return;
            }
            auto di = double(i);
            avg = r*( 1. / di ) + avg*( double( i - 1 ) / di );
        };
    } ();
    auto inc_hull_points_idx = [&hull_points_vec] (int i) {
        assert(i >= 0 && i < int(hull_points_vec.size()));
        return (i + 1) % int(hull_points_vec.size());
    };
    auto get_hull_points = [&hull_points_vec, &inc_hull_points_idx](int i) {
        assert(i >= 0 && i < int(hull_points_vec.size()));
        auto hp_f = hull_points_vec[i];
        auto hp_s = hull_points_vec[inc_hull_points_idx(i)];
        return std::make_tuple(hp_f, hp_s);
    };
    for_each_holes_and_fills(
        bezier_triples.begin(), bezier_triples.end(),
        [](BezierTripleIter, BezierTripleIter, int) { /* skip holes */ },
        [&avg_taker, &nhull_center, &inc_hull_points_idx, &get_hull_points]
        (BezierTripleIter beg, BezierTripleIter end, int idx)
    {
        for (auto itr = beg; itr != end; ++itr) {
            avg_taker(nhull_center, VectorD( std::get<0>(get_hull_points(idx)) ));
            idx = inc_hull_points_idx(idx);
        }
        if (beg != end) {
            avg_taker(nhull_center, VectorD( std::get<1>(get_hull_points(idx))));
        }
    });
    return nhull_center;
}
#endif
#if 0
//std::vector<BezierTriple> make_back_curves(const std::vector<BezierTriple> &, VectorD hull_center);

// handles holes, makes a single coherent, renderable shape
std::vector<BezierTriple> make_front_curves
    (const std::vector<BezierTriple> &, VectorD adjusted_hull_center, VectorD root_pos);
#endif
#if 0
Grid<bool> make_foilage_web_mask(int width, int height, const std::vector<BezierTriple> &);
#endif
#if 0
std::vector<BezierTriple> make_front_curves
    (const std::vector<BezierTriple> & bz_triples,
     VectorD adjusted_hull_center, VectorD root_pos)
{
    using std::get;
    VectorD ex_center = (adjusted_hull_center + root_pos)*0.5;
    double ex_radius = std::max(magnitude(adjusted_hull_center - root_pos), 1.);
    // note: no points should be in the exclusion zone, defined as:
    // smallest possible circular region enclosing the adjusted_hull_center AND root pos

    // how do I know that the fill is enclosing the exclusion zone too?
    // cross product perhaps?
    static auto triple_is_real = [](const BezierTriple & triple) {
        return    is_real(get<0>(triple).x) && is_real(get<0>(triple).y)
               && is_real(get<1>(triple).x) && is_real(get<1>(triple).y)
               && is_real(get<2>(triple).x) && is_real(get<2>(triple).y);
    };
    std::vector<BezierTriple> triples;
    triples.reserve(bz_triples.size());
    // patch 1 sized holes with "new hull center"
    for_each_holes_and_fills(
        bz_triples.begin(), bz_triples.end(),
        [&triples, &bz_triples](BezierTripleIter beg, BezierTripleIter end, int idx) {
            if (end - beg == 1) {
                int prev_idx = idx - 1 < 0 ? int(bz_triples.size() - 1) : idx - 1;
                int next_idx = idx + 1 >= int(bz_triples.size()) ? 0 : idx + 1;
                auto avg = 0.5*(get<1>( bz_triples.at(prev_idx) ) +
                                get<1>( bz_triples.at(next_idx) ));
                auto trip = bz_triples[idx];
                get<1>(trip) = avg;
                //get<1>( triples[idx] ) = avg;
                triples.emplace_back(trip);
                assert(triple_is_real(triples.back()));
            } else {
                // multiples are way harder
                auto med = beg + ((end - beg) / 2);
                triples.emplace_back(get<0>(*(end - 1)), get<0>(*med), get<2>(*beg));
                assert(triple_is_real(triples.back()));
            }
        },
        [&triples] (BezierTripleIter beg, BezierTripleIter end, int)
    {
        triples.insert(triples.begin(), beg, end);
    });
    for (const auto & trip : triples)
        assert(triple_is_real(trip));
#   if 0
    auto end = triples.end();
    auto rem_end = std::remove_if(triples.begin(), end,
        [](const BezierTriple & triple)
    { return std::get<1>(triple) == k_no_location; });
    triples.erase(triples.begin(), rem_end);
    return triples;
#   endif
    return triples;
}
#endif
#if 0
Grid<bool> make_foilage_web_mask
    (int width, int height, const std::vector<BezierTriple> & triples)
{
    using std::get;
    Grid<bool> mold;
    mold.set_size(width, height, false);
    if (triples.empty()) return mold;
    auto do_line = [&mold](const VectorD a, VectorD b) {
        plot_bresenham_line(round_to<int>(a), round_to<int>(b), [&mold](VectorI r) { mold(r) = true; });
    };
    auto do_bezier = [&do_line](const BezierTriple & a) {
        for_bezier_lines(a, 1. / 5., do_line);
    };
    for_side_by_side_wrap(triples.begin(), triples.end(),
        [&do_bezier, &do_line](const BezierTriple & a, const BezierTriple & b)
    {

        auto bez_a = std::make_tuple(
            get<0>(a),
            (get<0>(a) + get<2>(a))*0.5,
            get<1>(a));
        auto bez_b = std::make_tuple(
            get<1>(a),
            (get<0>(a) + get<2>(a))*0.5,
            get<2>(a));
        //do_bezier(a);
        do_bezier(bez_a);
        do_bezier(bez_b);
        do_line(get<2>(a), get<0>(b));
    });
    Grid<bool> mask;
    mask.set_size(width, height, false);
    iterate_grid_group(
        make_sub_grid(mold), VectorI(width / 2, height / 2),
        [&mold](VectorI r) { return !mold(r); },
        [&mask](VectorI r, bool) { mask(r) = true; });
    return mask;
}
#endif
#if 0
void draw_disk(SubGrid<sf::Color> target, VectorI r, int radius, sf::Color color) {
    const VectorI k_center(radius, radius);
    auto subg = make_sub_grid(target, r - VectorI(1, 1)*radius, radius*2, radius*2);
    for (VectorI v; v != subg.end_position(); v = subg.next(v)) {
        auto diff = v - k_center;
        if (diff.x*diff.x + diff.y*diff.y >= radius*radius) continue;
#       if 0
        if (color.a != 255) {
            int j = 0;
            ++j;
        }
#       endif
        auto c2 = color;
        if (color.a != 255) c2.a = ((v.x + v.y / 4) % 2) ? 255 : color.a;
        subg(v) = c2;
    }
}
#endif
#if 0
Grid<sf::Color> generate_leaves(int width, int height, int radius, int count) {
    static constexpr const int k_max_width  = 512;
    static constexpr const int k_max_height = k_max_width;
#   if 0
    static const auto k_pallete = {
        sf::Color(230, 60, 0), sf::Color(200, 10, 0), sf::Color(180, 20, 20),
        sf::Color(230, 230, 0), sf::Color(200, 200, 0), sf::Color(180, 80, 20),
        sf::Color(150, 0, 0, 0)
    };
#   else
    static const auto k_pallete = {
        sf::Color(0, 230, 0), sf::Color(0, 200, 0), sf::Color(20, 180, 20),
        sf::Color(0, 230, 0), sf::Color(0, 200, 0), sf::Color(20, 180, 20),
        sf::Color(0, 150, 0, 0)
    };
#   endif
    static const auto k_foilage_texture = [] {
        std::default_random_engine rng { std::random_device()() };
        static constexpr const int k_radius = 4;
        Grid<sf::Color> rv;
        rv.set_size(k_max_width, k_max_height);
        static constexpr const int k_min_delta = k_radius*2 - 5;
        static constexpr const int k_max_delta = k_radius*2 - 1;
        static_assert(k_min_delta > 0, "");
#           if 0
        auto draw_disk = [&rv](VectorI r, sf::Color color) {
            static const VectorI k_center(k_radius, k_radius);
            auto subg = make_sub_grid(rv, r - VectorI(1, 1)*k_radius, k_radius*2, k_radius*2);
            for (VectorI r; r != subg.end_position(); r = subg.next(r)) {
                auto diff = r - k_center;
                if (diff.x*diff.x + diff.y*diff.y < k_radius*k_radius)
                    subg(r) = color;
            }
        };
#           endif
        auto delta_distri = std::uniform_int_distribution<int>(k_min_delta, k_max_delta);
        for (int y = k_max_height - k_radius; y > k_radius; y -= delta_distri(rng) / 2) {
            for (int x = k_radius; x < k_max_width - k_radius; x += delta_distri(rng)) {
                auto c = choose_random(rng, k_pallete);

                draw_disk(rv, VectorI(x, y), k_radius, c);
            }
        }
        return rv;
    } ();
    std::default_random_engine rng { std::random_device()() };
    std::vector<VectorI> bundle_points;
    bundle_points.reserve(count);
    make_bundle_locations(
        std::uniform_int_distribution<unsigned>()(rng),
        count, radius, width, height,
        [&bundle_points](VectorI r) { bundle_points.push_back(r); });
    Grid<BundleClass> class_grid;
    class_grid.set_size(width, height);
    classify_bundles(bundle_points, radius, class_grid, VectorI(width / 2, height / 2));
    auto hull_points = find_convex_hull(bundle_points);
    for (const auto & v : hull_points)
        class_grid(v).mark_as_hull();
    auto bezier_triples = make_triples(hull_points, class_grid, radius);
    auto adjusted_hull_center = find_hull_center_without_sunken(bezier_triples, hull_points);
    auto front_curves = make_front_curves(bezier_triples, adjusted_hull_center, VectorD(width / 2, height / 2));
    auto mask = make_foilage_web_mask(width, height, front_curves);
    Grid<sf::Color> samp;
    samp.set_size(width, height, sf::Color(0, 0, 0, 0));

    for (VectorI r; r != samp.end_position(); r = samp.next(r)) {
        if (mask(r)) samp(r) = k_foilage_texture(r);
    }
    for (const auto & v : bundle_points) {
        if (class_grid(v).get_class() == tree_bundle_classes::k_body) continue;
        draw_disk(samp, v, radius, choose_random(rng, k_pallete));
    }
    for (const auto & v : bundle_points) {
        if (class_grid(v).get_class() != tree_bundle_classes::k_island) continue;
        plot_bresenham_line(v, VectorI(width /2, height /2), [&samp](VectorI r) { samp(r) = sf::Color(180, 140, 10); });
    }
    //to_image(samp).saveToFile("/media/ramdisk/test-foilagemask.png");
    return samp;
}
#endif
#if 0
const bool k_init_me = [] {
    [] {
        return;
    Grid<sf::Color> cgrid;
    cgrid.set_size(256, 256, sf::Color(0, 0, 0, 0));
    static constexpr const double k_rad = 80.;
    VectorI origin(cgrid.width() / 2, cgrid.height() / 2);
    for (double t = 0.; t < k_pi*2.; t += k_pi*2 / 36) {
        VectorI offset( round_to<int>(std::cos(t)*k_rad), round_to<int>(std::sin(t)*k_rad) );
        plot_bresenham_line(origin, origin + offset, [&cgrid](VectorI r) {
            cgrid(r) = sf::Color::Red;
        });
    }
    sf::Image img;
    img.create(unsigned(cgrid.width()), unsigned(cgrid.height()), sf::Color(0, 0, 0, 0));
    for (VectorI r; r != cgrid.end_position(); r = cgrid.next(r)) {
        img.setPixel(unsigned(r.x), unsigned(r.y), cgrid(r));
    }
    img.saveToFile("/media/ramdisk/bresenham-test.png");
    } ();
    [] {
        return;
        Grid<sf::Color> cgrid;
        cgrid.set_size(256, 256, sf::Color(0, 0, 0, 0));
        static const auto k_color = sf::Color(30, 200, 30);
        auto bz_pts = std::make_tuple(VectorD(0, 255), VectorD(100, 20), VectorD(150, 1), VectorD(180, 50));
        auto do_bz = [&bz_pts, &cgrid] {
            std::cout << "(" << std::get<0>(bz_pts) << ")"
                      << "(" << std::get<1>(bz_pts) << ")"
                      << "(" << std::get<2>(bz_pts) << ")"
                      << "(" << std::get<3>(bz_pts) << ")"
                      << std::endl;
            for_bezier_lines(bz_pts, 1. / 20., [&cgrid](VectorD a, VectorD b) {
                plot_bresenham_line(round_to<int>(a), round_to<int>(b), [&cgrid] (VectorI r) {
                    if (!cgrid.has_position(r)) return;
                    cgrid(r) = k_color;
                });
            });
        };

        do_bz();
        {
        using std::get;

        auto c = find_closest_point_to_line(get<0>(bz_pts), get<3>(bz_pts), get<1>(bz_pts));
        get<1>(bz_pts) -= (get<1>(bz_pts) - c)*2.;
        c = find_closest_point_to_line(get<0>(bz_pts), get<3>(bz_pts), get<2>(bz_pts));
        get<2>(bz_pts) -= (get<2>(bz_pts) - c)*2.;

        }

        do_bz();

        Grid<bool> fill_this;
        fill_this.set_size(cgrid.width(), cgrid.height(), false);
        iterate_grid_group(make_sub_grid(cgrid), VectorI(128, 128),
            [&cgrid](VectorI r) { return cgrid(r) == sf::Color(0, 0, 0, 0); },
            [&fill_this](VectorI r, bool) { fill_this(r) = true; });
        for (VectorI r; r != cgrid.end_position(); r = cgrid.next(r)) {
            if (fill_this(r)) cgrid(r) = sf::Color::Red;
        }


        to_image(cgrid).saveToFile("/media/ramdisk/bez-test.png");
    } ();
    [] {
        return;
        Spine s;
        {
        Tag t;
        t.set_direction(normalize(VectorD(1, -1))).set_location(VectorD(40, 0))
         .set_width_angle(k_pi*0.1667);
        Anchor a;
        a.set_direction(normalize(VectorD(0.1, -1))).set_length(90.).set_width(50.)
         .set_location(VectorD(180, 250));
        s.set_anchor(a);
        s.set_tag(t);
        }

        using BezierTuple = Spine::BezierTuple;
        Grid<sf::Color> grid;
        grid.set_size(256, 256, sf::Color(0, 0, 0, 0));
        static const sf::Color k_color(0, 80, 200);
        s.render_to(make_spine_renderer(
            [&grid](const BezierTuple & left, const BezierTuple & right)
        {
            for (const auto & bz_pts : { left, right }) {
            for_bezier_lines(bz_pts, 1. / 20., [&grid](VectorD a, VectorD b) {
            plot_bresenham_line(round_to<int>(a), round_to<int>(b), [&grid] (VectorI r) {
                if (!grid.has_position(r)) return;
                grid(r) = k_color;
            }); }); }
        }));
        plot_bresenham_line(
            round_to<int>(std::get<0>(s.anchor().left_points ())),
            round_to<int>(std::get<0>(s.anchor().right_points())),
            [&grid](VectorI r)
        {
            grid(r) = k_color;
        });

        iterate_grid_group(make_sub_grid(grid),
            round_to<int>(s.anchor().location() + VectorD(0, -2)),
            [&grid](VectorI r) { return grid(r) == sf::Color(0, 0, 0, 0); },
            [&grid](VectorI r, bool) { grid(r) = k_color; });

        to_image(grid).saveToFile("/media/ramdisk/spine-test.png");
    } ();
    [] {
        return ;
        Grid<sf::Color> grid;
        grid.set_size(400, 300, sf::Color(0, 0, 0, 0));

        std::default_random_engine rng { std::random_device()() };
        for_ellip_distri(Rect(50, 50, 400 - 100, 300 - 100), 70000, rng, [&grid](VectorD r) {
            grid(round_to<int>(r)) = sf::Color::Red;
        });
        to_image(grid).saveToFile("/media/ramdisk/radial-test.png");
    } ();
#   if 0
    [] {
        std::vector a { VectorI(0, 0), VectorI(1, 0), VectorI(0, 1) };
        insert_uncrossed(a, VectorI(1, 1));

        std::vector b { VectorI(349, 149), VectorI(143, 223), VectorI(206, 249),
                        VectorI(172,  66) };
        insert_uncrossed(b, VectorI(102, 184));
        int j = 0;
        ++j;
    } ();
#   endif
    [] {
        return;
        Grid<sf::Color> grid;
        grid.set_size(400, 300, sf::Color(0, 0, 0, 0));

        std::vector<VectorI> pts;
        std::default_random_engine rng { std::random_device()() };
        for_ellip_distri(Rect(50, 50, 400 - 100, 300 - 100), 70000, rng, [&grid, &pts](VectorD r) {
            pts.push_back(round_to<int>(r));
            grid(round_to<int>(r)) = sf::Color::Red;
        });
        auto hull = find_convex_hull(pts);
        auto set_line_pt = [&grid] (VectorI r) { grid(r) = sf::Color::Blue; };
        for_side_by_side(hull, [&set_line_pt](VectorI a, VectorI b) {
            plot_bresenham_line(a, b, set_line_pt);
        });
        if (!hull.empty()) {
            plot_bresenham_line(hull.back(), hull.front(), set_line_pt);
        }
        to_image(grid).saveToFile("/media/ramdisk/convexhull.png");
    } ();
    [] {
        {
            auto k_test_cont = { 0, 1, 2, 3, 4, 5 };
            int count = 0;
            for_side_by_side_wrap(k_test_cont.begin(), k_test_cont.end(), [&count](int, int) {
                ++count;
            });
            assert(count == int(k_test_cont.size()));
        }
    } ();
#   if 0
    [] (int width, int height, int radius, int count) {
        static constexpr const int k_max_width  = 512;
        static constexpr const int k_max_height = k_max_width;
        static const auto k_pallete = {
            sf::Color(0, 230, 0), sf::Color(0, 200, 0), sf::Color(20, 180, 20)
        };
        static const auto k_foilage_texture = [] {
            std::default_random_engine rng { std::random_device()() };
            static constexpr const int k_radius = 6;
            Grid<sf::Color> rv;
            rv.set_size(k_max_width, k_max_height);
            static constexpr const int k_min_delta = 7;
            static constexpr const int k_max_delta = 11;
#           if 0
            auto draw_disk = [&rv](VectorI r, sf::Color color) {
                static const VectorI k_center(k_radius, k_radius);
                auto subg = make_sub_grid(rv, r - VectorI(1, 1)*k_radius, k_radius*2, k_radius*2);
                for (VectorI r; r != subg.end_position(); r = subg.next(r)) {
                    auto diff = r - k_center;
                    if (diff.x*diff.x + diff.y*diff.y < k_radius*k_radius)
                        subg(r) = color;
                }
            };
#           endif
            auto delta_distri = std::uniform_int_distribution<int>(k_min_delta, k_max_delta);
            for (int y = k_max_height - k_radius; y > k_radius; y -= delta_distri(rng) / 2) {
                for (int x = k_radius; x < k_max_width - k_radius; x += delta_distri(rng)) {
                    draw_disk(rv, VectorI(x, y), k_radius, choose_random(rng, k_pallete));
                }
            }
            return rv;
        } ();
        std::default_random_engine rng { std::random_device()() };
        std::vector<VectorI> bundle_points;
        bundle_points.reserve(count);
        make_bundle_locations(
            std::uniform_int_distribution<unsigned>()(rng),
            count, radius, width, height,
            [&bundle_points](VectorI r) { bundle_points.push_back(r); });
        Grid<BundleClass> class_grid;
        class_grid.set_size(width, height);
        classify_bundles(bundle_points, radius, class_grid, VectorI(width / 2, height / 2));
        auto hull_points = find_convex_hull(bundle_points);
        for (const auto & v : hull_points)
            class_grid(v).mark_as_hull();
        auto bezier_triples = make_triples(hull_points, class_grid, radius);
        auto adjusted_hull_center = find_hull_center_without_sunken(bezier_triples, hull_points);
        auto front_curves = make_front_curves(bezier_triples, adjusted_hull_center, VectorD(width / 2, height / 2));
        auto mask = make_foilage_web_mask(width, height, front_curves);
        Grid<sf::Color> samp;
        samp.set_size(width, height, sf::Color(0, 0, 0, 0));

        for (VectorI r; r != samp.end_position(); r = samp.next(r)) {
            if (mask(r)) samp(r) = k_foilage_texture(r);
        }
        for (const auto & v : bundle_points) {
            draw_disk(samp, v, radius, choose_random(rng, k_pallete));
        }

        to_image(samp).saveToFile("/media/ramdisk/test-foilagemask.png");
    } (320, 240, 10, 48);
#   endif
#   if 0
    auto genl = [] {
        static constexpr const int k_radius = 8;
        static constexpr const int k_width  = 400;
        static constexpr const int k_height = 300;
        std::default_random_engine rng { std::random_device()() };
        std::vector<VectorI> points;
        points.reserve(10);

        for_ellip_distri(
            Rect(k_radius, k_radius, k_width - k_radius*2, k_height - k_radius*2),
            points.capacity(), rng, [&points](VectorD r)
        { points.push_back(round_to<int>(r)); });

        std::unordered_set<VectorI, HashVector> hull_points_set;
        auto hull_points_vec = find_convex_hull(points);
        for (auto v : hull_points_vec) {
            bool b = hull_points_set.insert(v).second;
            assert(b);
        }
        assert(hull_points_set.size() == hull_points_vec.size());

        Grid<int> in_body;
        in_body.set_size(k_width, k_height, 0);
        for (const auto & v : points) {
            auto subg = make_sub_grid(in_body, v - VectorI(1, 1)*k_radius, k_radius*2, k_radius*2);
            static const VectorI k_sub_center(k_radius, k_radius);
            auto mark_num = hull_points_set.find(v) == hull_points_set.end() ? 1 : 3;
            for (VectorI r; r != subg.end_position(); r = subg.next(r)) {
                if (magnitude(r - k_sub_center) <= k_radius) subg(r) = mark_num;
            }
        }
#       if 1
        iterate_grid_group(make_sub_grid(in_body),
            VectorI(in_body.width() / 2, in_body.height() / 2),
            [&in_body](VectorI r){ return in_body(r) != 0; },
            [&in_body](VectorI r, bool){ in_body(r) = 2; });
#       endif
        Grid<sf::Color> img;
        img.set_size(in_body.width(), in_body.height(), sf::Color(0, 0, 0, 0));
        auto get_color = [](int i) {
            static const auto k_colors = { sf::Color(0, 0, 0, 0), sf::Color(200, 40, 40), sf::Color(40, 200, 40), sf::Color(40, 40, 200) };
            assert(i >= 0 && i < int(k_colors.size()));
            return *(k_colors.begin() + i);
        };
        for (VectorI r; r != in_body.end_position(); r = in_body.next(r)) {
            img(r) = get_color(in_body(r));
        }

        std::vector<BezierTriple> bezier_triples =
            [] (const std::vector<VectorI> & hull_points_vec)
        {
        auto hull_center = find_center<int>(hull_points_vec.begin(), hull_points_vec.end());
        auto opposites = find_opposite_sides<true>(hull_points_vec, hull_center);
#       if 0
        static const VectorD k_no_pos(k_inf, k_inf);
#       endif
        // dips are the middle points
        std::vector<BezierTriple> bezier_triples;
        bezier_triples.reserve(hull_points_vec.size());
        for_side_by_side_wrap(hull_points_vec.begin(), hull_points_vec.end(),
            [&img, &hull_center, &bezier_triples, &opposites, &hull_points_vec](const VectorI & a, const VectorI & b)
        {
            auto para = normalize(get_hull_out_of_segment(VectorD(a), VectorD(b), VectorD(hull_center)));
            auto mid = (VectorD(a) + VectorD(b))*0.5 + para;
            // step until we hit an occupied pixel or we cross the "opposite" side
#           if 0
            static const VectorD k_no_dip_pos(k_inf, k_inf);
#           endif
            VectorD dip_pos = k_no_location; // k_no_dip_pos;
            VectorD step;
            double stop_distance;
            {
            auto itr = *(opposites.begin() + (&a - &hull_points_vec.front()));
            VectorD stop_a { *itr };
            VectorD stop_b { itr + 1 == hull_points_vec.end() ? *hull_points_vec.begin() : *(itr + 1) };
            auto to_stop = ( (stop_a + stop_b)*0.5 ) - mid;
            assert( find_intersection(stop_a, stop_b, mid, mid + to_stop*1.1) != k_no_intersection );
            step = normalize(to_stop);
            stop_distance = magnitude(to_stop);
            }
            for (auto pos = mid + step; magnitude(pos - mid) < stop_distance; pos += step)
            {
                if (img(round_to<int>(pos)) != sf::Color(0, 0, 0, 0)) {
                    dip_pos = pos;
                    break;
                }
            }
            bezier_triples.emplace_back(
                VectorD(a) + para*double(k_radius),
                dip_pos,
                VectorD(b) + para*double(k_radius));
        });
        assert(bezier_triples.size() == hull_points_vec.size());
        return bezier_triples;
        } (hull_points_vec);
        int total = 0;
        for_each_holes_and_fills(
            bezier_triples.begin(), bezier_triples.end(),
            [&total](BezierTripleIter beg, BezierTripleIter end, int) {
                total += (end - beg);
                std::cout << "holes " << (end - beg) << " ";
            }, [&total](BezierTripleIter beg, BezierTripleIter end, int) {
                total += (end - beg);
                std::cout << "fills " << (end - beg) << " ";
            });
        assert(total == int(bezier_triples.size()));
        std::cout << std::endl;
        // triples...
        // triples.begin() corresponds to hull pt begin() and begin() + 1
#       if 0
        auto get_linked_hull_points = [&hull_points_vec, &bezier_triples] (BezierTripleIter itr) {
            assert(itr >= bezier_triples.begin() && itr < bezier_triples.end());
            auto idx = itr - bezier_triples.begin();
            auto hp_itr = hull_points_vec.begin() + idx;
            auto hp_end = hull_points_vec.end();
            return std::make_tuple(*hp_itr, *(hp_itr + 1 == hp_end ? hull_points_vec.begin() : hp_itr + 1));
        };
#       endif
        VectorD nhull_center =
        [] (const std::vector<BezierTriple> & bezier_triples, const std::vector<VectorI> & hull_points_vec) {
            VectorD nhull_center = k_no_location;
        auto avg_taker = [] {
            //double t = 0.;
            int i = 0;
            return [i] (VectorD & avg, VectorD r) mutable {
                if (i++ == 0) {
                    avg = r;
                    return;
                }
                auto di = double(i);
                avg = r*( 1. / di ) + avg*( double( i - 1 ) / di );
            };
        } ();
        auto inc_hull_points_idx = [&hull_points_vec] (int i) {
            assert(i >= 0 && i < int(hull_points_vec.size()));
            return (i + 1) % int(hull_points_vec.size());
        };
        auto get_hull_points = [&hull_points_vec, &inc_hull_points_idx](int i) {
            assert(i >= 0 && i < int(hull_points_vec.size()));
            auto hp_f = hull_points_vec[i];
            auto hp_s = hull_points_vec[inc_hull_points_idx(i)];
            return std::make_tuple(hp_f, hp_s);
        };
        for_each_holes_and_fills(
            bezier_triples.begin(), bezier_triples.end(),
            [](BezierTripleIter, BezierTripleIter, int) { /* skip holes */ },
            [&avg_taker, &nhull_center, &inc_hull_points_idx, &get_hull_points]
            (BezierTripleIter beg, BezierTripleIter end, int idx)
        {
            for (auto itr = beg; itr != end; ++itr) {
                avg_taker(nhull_center, VectorD( std::get<0>(get_hull_points(idx)) ));
                idx = inc_hull_points_idx(idx);
            }
            if (beg != end) {
                avg_taker(nhull_center, VectorD( std::get<1>(get_hull_points(idx))));
            }
        });
        return nhull_center;
        } (bezier_triples, hull_points_vec);
        // patch 1 sized holes with "new hull center"
        for_each_holes_and_fills(
            bezier_triples.begin(), bezier_triples.end(),
            [/*&nhull_center,*/ &bezier_triples](BezierTripleIter beg, BezierTripleIter end, int idx) {
                if (end - beg > 1) return;
                int prev_idx = idx - 1 < 0 ? int(bezier_triples.size() - 1) : idx - 1;
                int next_idx = idx + 1 >= int(bezier_triples.size()) ? 0 : idx + 1;
                auto avg = 0.5*(std::get<1>( bezier_triples.at(prev_idx) ) +
                                std::get<1>( bezier_triples.at(next_idx) ));
                std::get<1>( bezier_triples[idx] ) = avg; //nhull_center;
            },
            [] (BezierTripleIter, BezierTripleIter, int){});
        {
        auto end = bezier_triples.end();
        auto rem_end = std::remove_if(bezier_triples.begin(), end,
            [](const BezierTriple & triple)
        { return std::get<1>(triple) == k_no_location; });
        bezier_triples.erase(bezier_triples.begin(), rem_end);
        }
        auto set_pixel = [&img](VectorI r) { img(r) = sf::Color::Cyan; };
        VectorD first_point = k_no_location;
        VectorD last_point = k_no_location;
        int count = 0;
        for (const auto & triple : bezier_triples) {
            using std::get;
            // probably should handle "holes" here as opposed to the very end
            if (get<1>(triple) == k_no_location) continue;
            ++count;
            if (first_point == k_no_location) first_point = get<0>(triple);
            if (last_point != k_no_location) {
                plot_bresenham_line(
                    round_to<int>(last_point), round_to<int>(get<0>(triple)),
                    set_pixel);
            }
            for_bezier_lines(triple, 1. / 5., [&set_pixel](VectorD a, VectorD b) {
                plot_bresenham_line(round_to<int>(a), round_to<int>(b), set_pixel);
            });
            last_point = get<2>(triple);
        }
        if (count == 1) {
            plot_bresenham_line(round_to<int>(first_point), round_to<int>(nhull_center),
                                set_pixel);
            plot_bresenham_line(round_to<int>(last_point), round_to<int>(nhull_center),
                                set_pixel);
        } else if (count > 1) {
            plot_bresenham_line(round_to<int>(first_point), round_to<int>(last_point),
                                set_pixel);
        }
#       if 0
        auto hc_pt = round_to<int>(nhull_center);
        auto subg = make_sub_grid(img, hc_pt - VectorI(1, 1)*10, 20, 20);
        for (auto & clr : subg) {
            clr = sf::Color::Cyan;
        }
#       endif
        return img;
        //to_image(img).saveToFile("/media/ramdisk/group-test.png");
    };

    for (auto i = 0; i != 20; ++i) {
        auto img = genl();
        std::string fn = "/media/ramdisk/group-test";
        if (i < 10) fn += "0";
        fn += std::to_string(i);
        fn += ".png";
        to_image(img).saveToFile(fn);
    }
#   endif
    return true;
} ();
#endif
static bool is_comma(char c) { return c == ','; }
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
#if 0
template <typename Func>
void for_each_pixel(const Spine & spine, Func && f) {
    using BezierTuple = Spine::BezierTuple;
    spine.render_to(make_spine_renderer(
        [&f](const BezierTuple & left, const BezierTuple & right)
    {
        for (const auto & bz_pts : { left, right }) {
        for_bezier_lines(bz_pts, 1. / 20., [&f](VectorD a, VectorD b) {
            plot_bresenham_line(round_to<int>(a), round_to<int>(b), f);
        }); }
    }));
}
#endif
#if 0
template <typename T>
class ConstrainedNormalDistribution {
public:
    static constexpr const T k_default_maxstddevs = 2.;
    ConstrainedNormalDistribution(T min, T max, T stddev, T maxstddevs = k_default_maxstddevs);

    template <typename Urng>
    T operator () (Urng & rng) const {

    }
private:
    T cdf(T t) const;

    T find_cdf_inv(T t) const;


};
#endif

// ----------------------------------------------------------------------------

void GraphicsDrawer::render_to(sf::RenderTarget & target) {
    render_back(target);
    render_front(target);
}

void GraphicsDrawer::render_front(sf::RenderTarget & target) {
    m_map_decor.render_front(target);
}

void GraphicsDrawer::render_back(sf::RenderTarget & target) {
    m_map_decor.render_back(target);
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

void GraphicsDrawer::load_decor(const tmap::TiledMap & map) {
    m_map_decor.load_map(map);
}

/* private */ void GraphicsDrawer::draw_sprite(const sf::Sprite & spt) {
    Rect spt_rect;
    spt_rect.left = spt.getPosition().x;
    spt_rect.top  = spt.getPosition().y;
    spt_rect.width = spt.getTextureRect().width;
    spt_rect.height = spt.getTextureRect().height;
    if (!spt_rect.intersects(m_view_rect)) return;
    m_sprites.push_back(spt);
    auto s = m_sprites.size();
    int j = 0;
    ++j;
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
