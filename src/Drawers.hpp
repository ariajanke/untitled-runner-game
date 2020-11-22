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

#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Vertex.hpp>

#include <vector>

namespace sf { class RenderTarget; }

struct LineDrawer {
    using VectorD = sf::Vector2<double>;
    LineDrawer(): target(nullptr), thickness(0.0) {}
    void draw_line(VectorD, VectorD) const;
    void draw_line(VectorD, VectorD, sf::Color) const;
    void draw_line(sf::RenderTarget &, VectorD, VectorD) const;
    void draw_line(sf::RenderTarget &, VectorD, VectorD, sf::Color) const;
    sf::RenderTarget * target;
    double thickness;
    sf::Color color;
};

class CircleDrawer {
public:
    using VectorD = sf::Vector2<double>;
    CircleDrawer(): m_radius(20.), m_pixels_per_point(10.), m_target(nullptr) {}
    void set_pixels_per_point(double);
    void assign_render_target(sf::RenderTarget *);
    void set_radius(double);
    void draw_circle(VectorD) const;
    void draw_circle(sf::RenderTarget &, VectorD) const;
    void set_color(sf::Color);
private:
    void update_verticies();
    std::vector<sf::Vertex> m_vertices;
    double m_radius;
    double m_pixels_per_point;
    sf::RenderTarget * m_target;
    sf::Color m_color;
};
