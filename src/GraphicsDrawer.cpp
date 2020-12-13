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

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>

#include <tmap/TiledMap.hpp>

#include <iostream>

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

void MapDecorDrawer::load_map(const tmap::TiledMap & tmap) {
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

        Flower flower;
        flower.setup(rng);
        flower.set_location(plant_location - VectorD(flower.width()*0.5, flower.height()));
        m_flowers.emplace_back(flower);
    }}
}

void MapDecorDrawer::render_to(sf::RenderTarget & target) const {
    Rect draw_bounds(VectorD(target.getView().getCenter() - target.getView().getSize()*0.5f),
                     VectorD(target.getView().getSize()));
    for (const auto & flower : m_flowers) {
        Rect flower_bounds(flower.location(), VectorD(flower.width(), flower.height()));
        if (!draw_bounds.intersects(flower_bounds)) continue;
        target.draw(flower);
    }
}

void MapDecorDrawer::update(double et) {
    for (auto & flower : m_flowers) {
        flower.update(et);
    }
}

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

void GraphicsDrawer::render_to(sf::RenderTarget & target) {
    m_map_decor.render_to(target);
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
