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

namespace tmap {
    class TiledMap;
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

View<const sf::Vertex *> get_unit_circle_verticies_for_radius(double radius);

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
        cul::DrawRectangle draw_rect;
        VectorD start, end;
        double time_passed;
    };
    std::unordered_map<ecs::EntityRef, Record, RefHasher> m_flag_records;
};

// ----------------------------------------------------------------------------

class BackgroundDrawer {
public:
    void setup();
    void set_view(const sf::View &);
    void render_to(sf::RenderTarget &) const;

private:
    struct Record {

    };
};

template <typename T>
std::enable_if_t<std::is_floating_point_v<T>, cul::Vector2<T>>
    to_unit_circle_vector(T t)
{ return cul::Vector2<T>(std::cos(t), std::sin(t)); }

template <typename T>
std::enable_if_t<std::is_floating_point_v<T>, T>
    to_direction(cul::Vector2<T> r)
{
    static const auto k_unit = to_unit_circle_vector(T(0));
    auto angle = angle_between(r, k_unit);
    auto samp = rotate_vector(k_unit, angle);
    if (are_very_close(samp, r))
        return angle;
    return k_pi*2. - angle;
}

class MapObjectLoader;

class MapDecorDrawer {
public:
    struct TempRes { virtual ~TempRes() {} };

    virtual ~MapDecorDrawer() {}
    virtual void update(double et) = 0;

    virtual void render_front(sf::RenderTarget &) const = 0;

    virtual void render_background(sf::RenderTarget &) const = 0;

    virtual void render_backdrop(sf::RenderTarget &) const = 0;

    void prepare_with_map(tmap::TiledMap & map, MapObjectLoader & objloader) {
        auto gv = prepare_map_objects(map, objloader);
        prepare_map(map, std::move(gv));
    }

    virtual void set_view_size(int width, int height) = 0;

protected:
    virtual std::unique_ptr<TempRes> prepare_map_objects(const tmap::TiledMap & tmap, MapObjectLoader &) = 0;

    virtual void prepare_map(tmap::TiledMap &, std::unique_ptr<TempRes>) {}

    MapDecorDrawer() {}
};

// ----------------------------------------------------------------------------

class VariablePlatformDrawer {
public:
    void prepare_texture(int max_length);

    // note it is (or at least should be possible) to have upside down platforms
    void draw_platform(VectorD left, VectorD right);

    void clear_platform_graphics();

    void render_front(sf::RenderTarget &) const;

    void render_background(sf::RenderTarget &) const;

private:
    std::vector<sf::Sprite> m_front_sprites, m_back_sprites;
    sf::Texture m_texture;
};

// ----------------------------------------------------------------------------

class GraphicsDrawer final : public GraphicsBase {
public:
    void render_front(sf::RenderTarget & target);

    void render_background(sf::RenderTarget & target);

    void render_backdrop(sf::RenderTarget &);

    void update(double et) {
        m_item_anis.update(et);
        m_flag_raiser.update(et);
        m_map_decor->update(et);
    }

    void set_view(const sf::View &);

    template <typename T>
    void take_decor(std::enable_if_t<std::is_base_of_v<MapDecorDrawer, T>, std::unique_ptr<T>> && uptr) {
        m_map_decor = std::move(uptr);
        m_platform_drawer.prepare_texture(400);
    }

private:
    void draw_line(VectorD a, VectorD b, sf::Color color, double thickness) override {
#       if 0
        if (!m_view_rect.contains(a) && !m_view_rect.contains(b)) return;
#       endif
        if (!is_contained_in(a, m_view_rect) && !is_contained_in(b, m_view_rect)) return;

        (void)color;
        (void)thickness;
        m_platform_drawer.draw_platform(a, b);
    }
    void draw_circle(VectorD loc, double radius, sf::Color color) override {
        Rect expanded = m_view_rect;
        expanded.left -= radius;
        expanded.top  -= radius;
        expanded.width += radius;
        expanded.height += radius;
#       if 0
        if (!expanded.contains(loc)) return;
#       endif
        if (!is_contained_in(loc, expanded)) return;
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
public:
    void reset_for_new_frame() override {
        // clear once-per-frames
        m_sprites.clear();
        m_draw_rectangles.clear();
        m_platform_drawer.clear_platform_graphics();
    }
private:
    Rect m_view_rect;
    CircleDrawer2 m_circle_drawer;
    LineDrawer2 m_line_drawer;
    std::vector<sf::Sprite> m_sprites;
    ItemCollectAnimations m_item_anis;
    std::vector<cul::DrawRectangle> m_draw_rectangles;
    FlagRaiser m_flag_raiser;

    std::unique_ptr<MapDecorDrawer> m_map_decor;

    VariablePlatformDrawer m_platform_drawer;
};
