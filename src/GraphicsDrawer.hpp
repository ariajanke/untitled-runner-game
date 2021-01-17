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

#include "TreeGraphics.hpp"

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

class Flower final : public sf::Drawable {
public:
    void setup(std::default_random_engine &);

    void update(double et);

    void set_location(double x, double y) { m_location = VectorD(x, y); }
    void set_location(VectorD r) { m_location = r; }

    VectorD location() const { return m_location; }

    double width() const { return m_petals.width(); }

    double height() const {
        // should be good enough
        return m_petals.height()*0.5 + m_stem.height();
    }

private:
    using UInt8Distri = std::uniform_int_distribution<uint8_t>;
    static constexpr const uint8_t k_max_u8 = std::numeric_limits<uint8_t>::max();

    // Just a really long time, I really want finite values
    static constexpr const double k_initial_time = 3600.;
    void draw(sf::RenderTarget &, sf::RenderStates) const override;

    double petal_thershold() const noexcept;

    double resettle_thershold() const noexcept;

    double pistil_pop_time() const noexcept;

    void pop_pistil(double amount);

    void pop_petal(double amount);

    static void verify_0_1_interval(const char * caller, double amt);

    static sf::Color random_petal_color(std::default_random_engine &);

    static sf::Color random_pistil_color(std::default_random_engine &);

    static sf::Color random_stem_color(std::default_random_engine &);

    DrawRectangle m_petals;
    DrawRectangle m_pistil;
    DrawRectangle m_stem  ;

    // animation works like this:
    // settled -> pistil pop -> petal pop -> settled

    double m_popped_position = 0.;

    double m_time               = 0.;
    double m_to_pistil_pop      = k_initial_time;
    double m_time_at_pistil_pop = k_initial_time;
    double m_to_petal_pop       = k_initial_time;
    double m_time_at_petal_pop  = k_initial_time;

    VectorD m_location;
};

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
std::enable_if_t<std::is_floating_point_v<T>, sf::Vector2<T>>
    to_unit_circle_vector(T t)
{ return sf::Vector2<T>(std::cos(t), std::sin(t)); }

template <typename T>
std::enable_if_t<std::is_floating_point_v<T>, T>
    to_direction(sf::Vector2<T> r)
{
    static const auto k_unit = to_unit_circle_vector(T(0));
    auto angle = angle_between(r, k_unit);
    auto samp = rotate_vector(k_unit, angle);
    if (are_very_close(samp, r))
        return angle;
    return k_pi*2. - angle;
}
#if 0
class Spine {
public:
    using BezierTuple = std::tuple<VectorD, VectorD, VectorD, VectorD>;

    class Tag {
    public:
        std::tuple<VectorD, VectorD> left_points() const;
        std::tuple<VectorD, VectorD> right_points() const;
        Tag & set_location(VectorD);
        Tag & set_direction(VectorD);
        Tag & set_width_angle(double);
        VectorD location() const;
        VectorD direction() const;

    private:
        std::tuple<VectorD, VectorD> points(double from_center) const;

        VectorD m_location;
        // [0 k_pi*2)
        double m_direction = 0.;
        // [0 k_pi*2)
        double m_width = 0.;
    };

    class Anchor {
    public:
        Anchor & set_location(VectorD);
        Anchor & set_berth(double);
        Anchor & set_width(double);
        // sets the amount which width decreases at the end of the anchor
        // trapazoids
        Anchor & set_pinch(double);
        Anchor & set_length(double);
        Anchor & set_direction(VectorD);
        void sway_toward(const Tag &);
        std::tuple<VectorD, VectorD> left_points() const
            { return get_points(-m_width*0.5); }
        std::tuple<VectorD, VectorD> right_points() const
            { return get_points( m_width*0.5); }
        VectorD location() const;
        VectorD direction() const;

    private:
        std::tuple<VectorD, VectorD> get_points(double offset) const;
        // [0 k_pi*2)
        double m_berth = 0.;
        // [0 m_berth)
        double m_angle = 0.;
        double m_length = 0.;
        double m_width = 0.;
        // [0 1)
        double m_pinch = 1.;
        double m_direction = 0.;
        VectorD m_location;
    };

    struct Renderer {
        virtual void render(const BezierTuple & left, const BezierTuple & right) const = 0;
    };

    void render_to(const Renderer & renderer) const {
        renderer.render(left_points(), right_points());
    }

    BezierTuple left_points() const {
        return std::tuple_cat( m_anchor.left_points (), m_tag.left_points () );
    }

    BezierTuple right_points() const {
        return std::tuple_cat( m_anchor.right_points (), m_tag.right_points () );
    }

    void set_anchor(const Anchor & anchor) { m_anchor = anchor; }

    void set_tag(const Tag & tag) { m_tag = tag; }

    void set_anchor_location(const VectorD & r)
        { (void)m_anchor.set_location(r); }

    void set_anchor_direction(const VectorD & r)
        { (void)m_anchor.set_direction(r); }

    void set_tag_location(const VectorD & r)
        { (void)m_tag.set_location(r); }

    void set_tag_direction(const VectorD & r)
        { (void)m_tag.set_direction(r); }

    const Anchor & anchor() const noexcept { return m_anchor; }

    const Tag & tag() const noexcept { return m_tag; }

private:
    Anchor m_anchor;
    Tag m_tag;
};

class PlantTree final : public sf::Drawable {
public:
    void plant
        (VectorD location, std::default_random_engine & rng, bool go_wide = true);

    void render_fronts(sf::RenderTarget &, sf::RenderStates) const;
    void render_backs(sf::RenderTarget &, sf::RenderStates) const;

    void save_to_file(const std::string &) const;

    Rect bounding_box() const noexcept;

private:
    void draw(sf::RenderTarget &, sf::RenderStates) const override;

    sf::Vector2f fore_leaves_location() const noexcept;
    sf::Vector2f trunk_adjusted_location() const noexcept;
    sf::Vector2f back_leaves_offset() const noexcept;

    sf::Texture m_trunk;
    sf::Texture m_fore_leaves;
    sf::Texture m_back_leaves;

    VectorD m_trunk_location;
    VectorD m_trunk_offset;
    VectorD m_leaves_location;
};
#endif
class MapDecorDrawer {
public:
    struct FutureTree {
#       if 0
        virtual void plant(VectorD location, std::default_random_engine) = 0;
#       endif
        virtual bool is_ready() const = 0;
        virtual bool is_done() const = 0;
        virtual PlantTree get_tree() = 0;
    };

    struct FutureTreeMaker {
        virtual ~FutureTreeMaker() {}
        virtual std::unique_ptr<FutureTree> make_tree
            (VectorD location, std::default_random_engine &) = 0;
    };

    void load_map(const tmap::TiledMap & tmap);

    [[deprecated]] void render_to(sf::RenderTarget &) const;

    void render_front(sf::RenderTarget &) const;

    void render_back(sf::RenderTarget &) const;

    void update(double et);

private:
    void plant_new_flower(std::default_random_engine &, VectorD);

    void plant_new_tree(std::default_random_engine &, VectorD);

    void plant_new_future_tree(std::default_random_engine &, VectorD);

    std::vector<Flower> m_flowers;
    std::vector<PlantTree> m_trees;
    std::vector<std::unique_ptr<FutureTree>> m_future_trees;

    std::unique_ptr<FutureTreeMaker> m_tree_maker;
};

#if 0
template <typename Func>
auto make_spine_renderer(Func && f) {
    using BezierTuple = Spine::BezierTuple;
    struct Rt final : public Spine::Renderer {
        Rt(Func && f): m_f(std::move(f)) {}
        void render(const BezierTuple & a, const BezierTuple & b) const override
            { m_f(a, b); }
        Func m_f;
    };
    return Rt(std::move(f));
}
#endif
// ----------------------------------------------------------------------------

class GraphicsDrawer final : public GraphicsBase {
public:
    [[deprecated]] void render_to(sf::RenderTarget & target);

    void render_front(sf::RenderTarget & target);

    void render_back(sf::RenderTarget & target);

    void update(double et) {
        m_item_anis.update(et);
        m_flag_raiser.update(et);
        m_map_decor.update(et);
    }

    void set_view(const sf::View &);

    void load_decor(const tmap::TiledMap &);

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

    MapDecorDrawer m_map_decor;
};
