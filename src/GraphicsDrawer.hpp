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

#include <common/DrawRectangle.hpp>

#include <SFML/Graphics/Vertex.hpp>

namespace sf {
    class RenderTarget;
    class View;
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

class FlagRaiser {
public:
    void post_flag_raise(ecs::EntityRef ref, VectorD bottom, VectorD top);

    void render_to(sf::RenderTarget &);

    void update(double et);

private:
    struct RefHasher {
        std::size_t operator () (const ecs::EntityRef & ref) const
            { return ref.hash(); }
    };
    struct Record {
        static constexpr const double k_raise_speed = 50.;
        static constexpr const double k_width  = 52.;
        static constexpr const double k_height = 28.;
        DrawRectangle draw_rect;
        VectorD start, end;
        double time_passed;
    };
    std::unordered_map<ecs::EntityRef, Record, RefHasher> m_flag_records;
};

// ----------------------------------------------------------------------------

class GraphicsDrawer final : public GraphicsBase {
public:
    void render_to(sf::RenderTarget & target);

    void update(double et) {
        m_item_anis.update(et);
        m_flag_raiser.update(et);
    }

    void set_view(const sf::View &);
private:
    void draw_line(VectorD a, VectorD b, sf::Color color, double thickness) override {
        if (!m_view_rect.contains(a) && !m_view_rect.contains(b)) return;
        m_line_drawer.post_line(a, b, color, thickness);
    }
    void draw_circle(VectorD loc, double radius, sf::Color color) override {
        Rect expanded = m_view_rect;
        expanded.left -= radius;
        expanded.top  -= radius;
        expanded.width += radius;
        expanded.height += radius;
        if (!expanded.contains(loc)) return;
        m_circle_drawer.post_circle(loc, radius, color);
    }

    void draw_sprite(const sf::Sprite & spt) override;

    void draw_holocrate(Rect, sf::Color) override {}

    void post_item_collection(VectorD r, AnimationPtr ptr) override
        { m_item_anis.post_effect(r, ptr); }

    void draw_rectangle
        (VectorD r, double width, double height, sf::Color color) override
    {
        m_draw_rectangles.emplace_back(float(r.x), float(r.y),
                                       float(width), float(height), color);
    }

    void post_flag_raise(ecs::EntityRef ref, VectorD bottom, VectorD top) override {
        m_flag_raiser.post_flag_raise(ref, bottom, top);
    }

    Rect m_view_rect;
    CircleDrawer2 m_circle_drawer;
    LineDrawer2 m_line_drawer;
    std::vector<sf::Sprite> m_sprites;
    ItemCollectAnimations m_item_anis;
    std::vector<DrawRectangle> m_draw_rectangles;
    FlagRaiser m_flag_raiser;
};
