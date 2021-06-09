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

#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/Texture.hpp>

class PlantTree final : public sf::Drawable {
public:
    using Rng = std::default_random_engine;

    struct RectSize {
        RectSize() {}
        RectSize(int w_, int h_): width(w_), height(h_) {}
        int width = 0, height = 0;
    };

    struct CreationParams {
        RectSize leaves_size;
        RectSize trunk_size;
        double   trunk_lean;
    };

    [[deprecated]] void plant
        (VectorD location, Rng &/*, bool go_wide = true*/);

    [[deprecated]] void plant(VectorD location, Rng &, const RectSize & leaves_size);

    void plant(VectorD location, const CreationParams &);

    static CreationParams generate_params(Rng &);

    void render_fronts(sf::RenderTarget &, sf::RenderStates) const;
    void render_backs(sf::RenderTarget &, sf::RenderStates) const;

    void save_to_file(const std::string &) const;

    Rect bounding_box() const noexcept;

    static RectSize choose_random_leaves_size(std::default_random_engine &);

    static VectorD leaves_location_from_params(const CreationParams &, VectorD plant_location);
#   if 0
    unsigned seed_value() const noexcept { return m_seed_value; }
#   endif
    // entity logic needs *deeper* knowledge still an entity when rustling leaves

    const Grid<bool> & front_leaves_bitmap() const
        { return m_front_leaves_bitmap; }

private:
    static constexpr const auto k_height_max   = 120.;
    static constexpr const auto k_height_min   =  70.;
    static constexpr const auto k_width_max    =  35.;
    static constexpr const auto k_width_min    =  18.;
    static constexpr const auto k_lean_max     = k_pi*0.16667;
    static const VectorD        k_lean_max_dir ;
    static constexpr const auto k_leaves_area  = 125.*125.;
    static constexpr const auto k_leaves_width_max = 160.;
    static constexpr const auto k_leaves_width_min = 120.;
    static constexpr const auto k_leaves_radius    =   8.;
    static constexpr const auto k_leaves_density   =  0.6;

    void draw(sf::RenderTarget &, sf::RenderStates) const override;

    sf::Vector2f fore_leaves_location() const noexcept;
    sf::Vector2f trunk_adjusted_location() const noexcept;
    sf::Vector2f back_leaves_offset() const noexcept;

    // I need to find a better way of figuring out seeding...
#   if 0
    unsigned m_seed_value = 0;
#   endif

    sf::Texture m_trunk;
    sf::Texture m_fore_leaves;
    sf::Texture m_back_leaves;

    Grid<bool> m_front_leaves_bitmap;

    VectorD m_trunk_location;
    VectorD m_trunk_offset;
    VectorD m_leaves_location;
};

// needed by tree
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

template <typename Func>
inline auto make_spine_renderer(Func && f) {
    using BezierTuple = Spine::BezierTuple;
    struct Rt final : public Spine::Renderer {
        Rt(Func && f): m_f(std::move(f)) {}
        void render(const BezierTuple & a, const BezierTuple & b) const override
            { m_f(a, b); }
        Func m_f;
    };
    return Rt(std::move(f));
}

// ----------------------------------------------------------------------------

template <typename T, typename Func, typename ... Types>
void for_bezier_points
    (const std::tuple<cul::Vector2<T>, Types...> &, T step, Func &&);

template <typename T, typename Func, typename ... Types>
void for_bezier_lines
    (const std::tuple<cul::Vector2<T>, Types...> &, T step, Func &&);

template <typename T, typename ... Types>
cul::Vector2<T> compute_bezier_point
    (T t, const std::tuple<cul::Vector2<T>, Types...> &);

// ----------------------------------------------------------------------------

template <typename T>
class BezierCurveDetails {

    template <typename U, typename Func, typename ... Types>
    friend void for_bezier_points
        (const std::tuple<cul::Vector2<U>, Types...> &, U step, Func &&);

    template <typename U, typename Func, typename ... Types>
    friend void for_bezier_lines
        (const std::tuple<cul::Vector2<U>, Types...> &, U step, Func &&);

    template <typename U, typename ... Types>
    friend cul::Vector2<U> compute_bezier_point
        (U t, const std::tuple<cul::Vector2<U>, Types...> &);

    using VecT = cul::Vector2<T>;

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
    (const std::tuple<cul::Vector2<T>, Types...> & tuple, T step, Func && f)
{ return BezierCurveDetails<T>::for_points(tuple, step, std::move(f)); }

template <typename T, typename Func, typename ... Types>
void for_bezier_lines
    (const std::tuple<cul::Vector2<T>, Types...> & tuple, T step, Func && f)
{ return BezierCurveDetails<T>::for_lines(tuple, step, std::move(f)); }

template <typename T, typename ... Types>
cul::Vector2<T> compute_bezier_point
    (T t, const std::tuple<cul::Vector2<T>, Types...> & tuple)
{ return BezierCurveDetails<T>::compute_point_tuple(t, tuple); }

// computes n points on a bezier curve according to a given tuple
template <std::size_t k_count, typename T, typename ... Types>
std::array<cul::Vector2<T>, k_count> make_bezier_array
    (const std::tuple<cul::Vector2<T>, Types...> & tuple)
{
    static constexpr const T k_step = T(1) / T(k_count);
    std::array<cul::Vector2<T>, k_count> arr;
    T t = T(0);
    for (auto & v : arr) {
        v = compute_bezier_point(std::min(T(1), t), tuple);
        t += k_step;
    }
    return arr;
}
