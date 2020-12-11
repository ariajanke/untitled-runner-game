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

#include "Defs.hpp"
#include "systems/SystemsDefs.hpp"

#include <SFML/Graphics/Vertex.hpp>

namespace sf {
    class RenderTarget;
}

class ItemCollectAnimations {
public:
    using AnimationPtr = std::shared_ptr<const ItemCollectionAnimation>;
    void post_effect(VectorD r, AnimationPtr aptr);
    void update(double et);
    void render_to(sf::RenderTarget & target) const;

private:
    struct Record {
        AnimationPtr ptr;
        std::vector<int>::const_iterator current_frame;
        double elapsed_time = 0.;
        VectorD location;
    };

    static bool should_delete(const Record & rec)
        { return rec.current_frame == rec.ptr->tile_ids.end(); }

    std::vector<Record> m_records;
};

// ----------------------------------------------------------------------------

class LineDrawer2 {
public:
    void post_line(VectorD a, VectorD b, sf::Color color, double thickness);
    // clears lines
    void render_to(sf::RenderTarget &);

private:
    std::vector<sf::Vertex> m_verticies;
};

// ----------------------------------------------------------------------------

class CircleDrawer2 {
public:
    void post_circle(VectorD r, double radius, sf::Color color);

    void render_to(sf::RenderTarget & target);

private:
    std::vector<sf::Vertex> m_vertices;
};

// ----------------------------------------------------------------------------

class GraphicsDrawer final : public GraphicsBase {
public:
    void render_to(sf::RenderTarget & target);

    void update(double et) {
        m_item_anis.update(et);
    }

private:
    void draw_line(VectorD a, VectorD b, sf::Color color, double thickness) override {
        m_line_drawer.post_line(a, b, color, thickness);
    }
    void draw_circle(VectorD loc, double radius, sf::Color color) override {
        m_circle_drawer.post_circle(loc, radius, color);
    }
    void draw_sprite(const sf::Sprite & spt) override {
        m_sprites.push_back(spt);
    }

    void draw_holocrate(Rect, sf::Color) override {}

    void post_item_collection(VectorD r, AnimationPtr ptr) override
        { m_item_anis.post_effect(r, ptr); }

    CircleDrawer2 m_circle_drawer;
    LineDrawer2 m_line_drawer;
    std::vector<sf::Sprite> m_sprites;
    ItemCollectAnimations m_item_anis;
};
