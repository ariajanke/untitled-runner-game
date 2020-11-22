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

#include "Drawers.hpp"

#include <common/Util.hpp>

#include <SFML/Graphics/PrimitiveType.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderTexture.hpp>

#include <stdexcept>

#include <cmath>
#include <cassert>

namespace {

using VectorD = sf::Vector2<double>;

VectorD add_polar(VectorD, double angle, double distance);

sf::Vertex to_vertex(VectorD, sf::Color);

} // end of <anonymous> namespace

void LineDrawer::draw_line(VectorD a, VectorD b) const {
    draw_line(a, b, color);
}

void LineDrawer::draw_line(VectorD a, VectorD b, sf::Color color) const {
    assert(target);
    draw_line(*target, a, b, color);
}

void LineDrawer::draw_line(sf::RenderTarget & target, VectorD a, VectorD b) const {
    draw_line(target, a, b, color);
}

void LineDrawer::draw_line
    (sf::RenderTarget & target, VectorD a, VectorD b, sf::Color color) const
{
    if (b.y > a.y) {
        draw_line(target, b, a, color);
        return;
    }
    static constexpr const auto k_quads = sf::PrimitiveType::Quads;

    auto init_angle = angle_between(a - b, VectorD(1., 0.));

    auto mk_vertex = [this, init_angle, &color](VectorD pt, double ang_dir) {
        auto theta = init_angle + ang_dir*(get_pi<double>() / 2.0);
        auto r = add_polar(pt, theta, thickness / 2.0);
        return to_vertex(r, color);
    };
    auto verticies = {
        mk_vertex(a,  1.0), mk_vertex(a, -1.0),
        mk_vertex(b, -1.0), mk_vertex(b,  1.0)
    };
    target.draw(&*verticies.begin(), verticies.size(), k_quads);
}

void CircleDrawer::set_pixels_per_point(double p) {
    if (p < 1.) {
        throw std::invalid_argument(
            "CircleDrawer::set_pixels_per_point: pixels per point must be a "
            "positive number greater than or equal to one. Non integers will "
            "be rounded.");
    }
    m_pixels_per_point = std::round(p);
    update_verticies();
}

void CircleDrawer::assign_render_target(sf::RenderTarget * target) {
    m_target = target;
}

void CircleDrawer::set_radius(double r) {
    if (r <= 0.) {
        throw std::invalid_argument(
            "CircleDrawer::set_radius: radius must be a positive real number.");
    }
    m_radius = r;
    update_verticies();
}

void CircleDrawer::set_color(sf::Color clr) {
    m_color = clr;
    for (auto & v : m_vertices)
        v.color = clr;
}

void CircleDrawer::draw_circle(VectorD pt) const {
    if (!m_target) return;
    draw_circle(*m_target, pt);
}

void CircleDrawer::draw_circle(sf::RenderTarget & target, VectorD pt) const {
    sf::RenderStates states;

    states.transform.translate(float(pt.x), float(pt.y));
    assert(m_vertices.size() % 3 == 0);
    for (auto itr = m_vertices.begin(); itr != m_vertices.end(); itr += 3) {
        target.draw(&*itr, 3, sf::PrimitiveType::Triangles, states);
    }
}

/* private */ void CircleDrawer::update_verticies() {
    m_vertices.clear();
    auto mk_vert = [this] (double x, double y) {
        sf::Vertex rv;
        rv.position.x = float(x);
        rv.position.y = float(y);
        rv.color      = m_color;
        return rv;
    };
    const auto step = m_pixels_per_point / m_radius;
    for (double t = 0.; t <= get_pi<double>()*2; t += step) {
        m_vertices.push_back(mk_vert(0., 0.));
        m_vertices.push_back(mk_vert(std::cos(t)*m_radius, std::sin(t)*m_radius));
        m_vertices.push_back(mk_vert(std::cos(t + step)*m_radius, std::sin(t + step)*m_radius));
    }
    m_vertices.back() = m_vertices[1];
    assert(m_vertices.size() % 3 == 0);
    std::reverse(m_vertices.begin(), m_vertices.end());
}

namespace {

VectorD add_polar(VectorD r, double angle, double distance) {
    return r + rotate_vector(VectorD(1.0, 0.0), angle)*distance;
}

sf::Vertex to_vertex(VectorD r, sf::Color color) {
    return sf::Vertex(sf::Vector2f(float(r.x), float(r.y)), color, sf::Vector2f());
}

} // end of <anonymous> namespace
