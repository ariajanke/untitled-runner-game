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
#include "GenBuiltinTileSet.hpp"

#include "FillIterate.hpp"
#include "maps/MapObjectLoader.hpp"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/RenderTexture.hpp>

#include <common/SubGrid.hpp>
#include <common/SfmlVectorTraits.hpp>

#include <tmap/TiledMap.hpp>
#include <tmap/TileSet.hpp>

#include <iostream>
#include <variant>
#include <unordered_set>
#include <thread>
#include <future>

#include <cassert>

namespace {

using cul::convert_to;
static const VectorD k_unit_start(1., 0.);

VectorD add_polar(VectorD r, double angle, double distance) {
    return r + rotate_vector(k_unit_start, angle)*distance;
}

sf::Vertex make_circle_vertex(double t)
    { return sf::Vertex(sf::Vector2f(float(std::cos(t)), float(std::sin(t)))); }

template <typename T>
const T * as_cptr(const T & obj) { return &obj; }

} // end of <anonymous> namespace

void LineDrawer2::post_line(VectorD a, VectorD b, sf::Color color, double thickness) {
    auto init_angle = angle_between(a - b, k_unit_start);

    auto mk_vertex = [thickness, init_angle, color](VectorD pt, double ang_dir) {
        auto theta = init_angle + ang_dir*(k_pi / 2.0);
        auto r = add_polar(pt, theta, thickness / 2.0);
        return sf::Vertex(convert_to<sf::Vector2f>(r), color);
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
    auto vertex_range = get_unit_circle_verticies_for_radius(radius);
    m_vertices.insert(m_vertices.end(), vertex_range.begin(), vertex_range.end());
    for (auto itr = get_begin(); itr != m_vertices.end(); ++itr) {
        itr->color = color;
        itr->position = float(radius)*itr->position + convert_to<sf::Vector2f>(r);
    }
}

void CircleDrawer2::render_to(sf::RenderTarget & target) {
    assert(m_vertices.size() % 3 == 0);
    target.draw(m_vertices.data(), m_vertices.size(), sf::PrimitiveType::Triangles);
    m_vertices.clear();
}

View<const sf::Vertex *> get_unit_circle_verticies_for_radius(double rad) {
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
            auto loc = convert_to<sf::Vector2f>(rec.start + offset);
            rec.draw_rect = cul::DrawRectangle(loc.x, loc.y, float(Record::k_width), height, sf::Color(50, 100, 200));

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
        brush.setPosition(convert_to<sf::Vector2f>(rec.location));
        brush.setTexture(rec.ptr->tileset->texture());
        brush.setTextureRect(rec.ptr->tileset->texture_rectangle(*rec.current_frame));
        target.draw(brush);
    }
}

// ----------------------------------------------------------------------------

std::string to_padded_string(int x) {
    if (x == 0) return "000";
    std::string rv = std::to_string(x);
    return std::string(std::size_t(2 - std::floor(std::log10(double(x)))), '0') + rv;
}

// ----------------------------------------------------------------------------

bool contains_point(const std::vector<VectorI> & points, VectorI center, VectorI pt) {
    using namespace cul::fc_signal;
    bool rv = false;
    auto in_stripe = [&] (VectorI a, VectorI b) {
        auto gv = find_intersection(VectorD(a), VectorD(b), VectorD(center), VectorD(pt));
        if (gv == k_no_intersection) {
            return k_continue;
        }
        rv = true;
        return k_break;
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

// ----------------------------------------------------------------------------

void VariablePlatformDrawer::prepare_texture(int max_length) {
    m_texture.loadFromImage(to_image(generate_platform_texture(max_length)));
}

void VariablePlatformDrawer::draw_platform(VectorD left, VectorD right) {
    using sf::IntRect;
    // yucky should come from builtin header
    static constexpr const int k_tile_size = 16;
    static const VectorD k_xi(1., 0);
    auto length = round_to<int>(magnitude(left - right));
    int length_in_first  = std::max(length - k_tile_size, length / 2);
    int length_in_second = length - length_in_first;

    sf::Sprite front_left, front_right, back_left, back_right;
    for (auto * spt : { &front_left, &front_right, &back_left, &back_right })
        { spt->setTexture(m_texture); }

    int revx  = int(m_texture.getSize().x) - length_in_second;
    int backy = k_tile_size*2;
    front_left .setTextureRect(IntRect(   0,     0, length_in_first , k_tile_size*2));
    back_left  .setTextureRect(IntRect(   0, backy, length_in_first , k_tile_size*2));
    front_right.setTextureRect(IntRect(revx,     0, length_in_second, k_tile_size*2));
    back_right .setTextureRect(IntRect(revx, backy, length_in_second, k_tile_size*2));

    for (auto * right : { &front_right, &back_right }) {
        right->setPosition(float(length_in_first), 0.f);
    }

    static const sf::Vector2f k_texture_offset(0.f, k_tile_size);
    sf::Vector2f origin = convert_to<sf::Vector2f>( (left + right)*0.5 ) + k_texture_offset;
    double angle_in_rads  = (std::atan2(right.y - left.y, right.x - left.x));
    float angle_to_rotate = float(angle_in_rads*(180. / k_pi));
    for (auto * spt : { &front_left, &front_right, &back_left, &back_right }) {
        spt->move(convert_to<sf::Vector2f>(left) - k_texture_offset);
        //spt->setOrigin(origin);
        //spt->rotate(angle_to_rotate);
    }

    m_front_sprites.insert(m_front_sprites.end(), { front_left, front_right });
    m_back_sprites .insert(m_back_sprites .end(), { back_left , back_right  });
}

void VariablePlatformDrawer::clear_platform_graphics() {
    m_front_sprites.clear();
    m_back_sprites .clear();
}

void VariablePlatformDrawer::render_front(sf::RenderTarget & target) const {
    for (auto & spt : m_front_sprites) {
        target.draw(spt);
    }
}

void VariablePlatformDrawer::render_background(sf::RenderTarget & target) const {
    for (auto & spt : m_back_sprites) {
        target.draw(spt);
    }
}

// ----------------------------------------------------------------------------

void GraphicsDrawer::render_front(sf::RenderTarget & target) {
    m_map_decor->render_front(target);

    m_platform_drawer.render_front(target);
    m_line_drawer.render_to(target);
}

void GraphicsDrawer::render_background(sf::RenderTarget & target) {
    m_map_decor->render_background(target);
    // m_line_drawer.render_to(target);
    m_circle_drawer.render_to(target);
    m_flag_raiser.render_to(target);
    for (const auto & spt : m_sprites) {
        target.draw(spt);
    }
    for (const auto & rect : m_draw_rectangles) {
        target.draw(rect);
    }
    m_item_anis.render_to(target);
    m_platform_drawer.render_background(target);
}

void GraphicsDrawer::render_backdrop(sf::RenderTarget & target) {
    m_map_decor->render_backdrop(target);
}

void GraphicsDrawer::set_view(const sf::View & view) {
    cul::set_top_left_of(m_view_rect, convert_to<VectorD>( view.getCenter() - view.getSize()*0.5f ));
    m_view_rect.width = double(view.getSize().x);
    m_view_rect.height = double(view.getSize().y);
#   if 0
    m_view_rect = Rect(VectorD( view.getCenter() - view.getSize()*0.5f), VectorD(view.getSize()));
#   endif
}

/* private */ void GraphicsDrawer::draw_sprite(const sf::Sprite & spt) {
    Rect spt_rect;
    spt_rect.left = spt.getPosition().x;
    spt_rect.top  = spt.getPosition().y;
    spt_rect.width = spt.getTextureRect().width;
    spt_rect.height = spt.getTextureRect().height;
    // no sprite culling I guess
    //if (!spt_rect.intersects(m_view_rect)) return;
    m_sprites.push_back(spt);
}
