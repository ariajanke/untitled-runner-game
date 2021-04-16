/****************************************************************************

    Copyright 2021 Aria Janke

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

#include "GraphicsDrawer.hpp"
#include "GraphicalEffects.hpp"
#include "TreeGraphics.hpp"
#include "Flower.hpp"

#include <set>

class SolarCycler final : public sf::Drawable {
public:
    enum TodEnum {
        k_sunrise_to_noon, k_noon_to_sunset, k_sunset_to_midnight,
        k_midnight_to_sunrise
    };

    using Rng = std::default_random_engine;
    static constexpr const double k_sample_day_length  = 30;
    static constexpr const double k_default_day_length = 60*60*24;
    static constexpr const int    k_default_star_count = 4000;

    void set_day_length  (double seconds);
    void update          (double seconds);
    void set_time_of_day (double seconds);
    void set_view_size   (int width, int height);

    /** @param star_count excludes the sun */
    void populate_sky    (Rng &, int star_count = k_default_star_count);

    enum ColorIndex { k_r, k_g, k_b, k_a, k_color_tuple_count };
    // there's only two possible specializations... so no templates
    using ColorTuple3 = std::tuple<double, double, double>;
    using ColorTuple4 = std::tuple<double, double, double, double>;
    static sf::Color mix_colors
        (double t, const ColorTuple3 & t_color, const ColorTuple3 & ti_color);
    static sf::Color mix_colors
        (double t, const ColorTuple4 & t_color, const ColorTuple4 & ti_color);

private:
    static uint8_t interpolate_u8(double t, double t_v, double ti_v);
    static double interpolate_sky_position(double);

    // Sun begins disappearing one radius below the horizon
    // Sun stops "growing" once it hits the radius
    // It seems natural that the Sun drives the whole cycle
    // future:
    // run more along a dome than in a circle
    class Sun final : public sf::Drawable {
    public:
        void set_center         (VectorD pixel_pos);
        /** @brief sets distance from origin/center */
        void set_offset         (double pixels);
        void set_position       (double radians);
        void set_radial_velocity(double radians_per_second);
        void update             (double seconds);

        /** @note should be useful for atmosphere coloring
         *  @return ToD and [0 1] */
        std::tuple<TodEnum, double> get_tod() const;

    private:        
        static           const sf::Color k_horizon_color;
        static constexpr const double    k_horizon_radius = 50.;
        static           const sf::Color k_zenith_color;
        static constexpr const double    k_zenith_radius  = 20. ;

    public:
        static constexpr const double k_max_radius =
            std::max(k_horizon_radius, k_zenith_radius);

    private:
        void draw(sf::RenderTarget &, sf::RenderStates) const override;
        void check_invariants() const;
        void ensure_vertices_present();

        sf::Color get_updated_color() const;
        double get_update_scale() const;
        double get_distance_to_horizon() const;

        std::vector<sf::Vertex> m_vertices;

        VectorD m_center;
        double m_offset          = 0.;
        double m_position        = 0.;
        double m_radial_velocity = 0.;
    };

    class Atmosphere final : public sf::Drawable {
    public:
        void set_size(int width, int height);
        void sync_to(const Sun &);

    private:
        static constexpr const int k_troposphere_height = 40;
        enum Altitude { k_bottom, k_middle, k_top, k_color_count };
        enum {
            k_top_left    ,
            k_top_right   ,
            k_bottom_right,
            k_bottom_left ,
            k_vertex_count
        };
        using VertexArray = std::array<sf::Vertex, k_vertex_count>;
        using ColorArray  = std::array<sf::Color , k_color_count >;

        /** @note it's private; implemented in source */
        template <typename Func>
        void for_each_color_altitude(Altitude, Func &&);

        /** @note it's private; implemented in source */
        template <typename Func>
        void for_each_altitude(Func &&);

        ColorArray get_colors_for_sun(const Sun &) const;

        void draw(sf::RenderTarget &, sf::RenderStates) const override;

        sf::Vector2f m_view_center;
        // thick gradient
        VertexArray m_troposphere;
        // flatter color
        VertexArray m_stratosphere;

        mutable TextDrawer m_postext;
    };

    class Star final {
    public:
        // low temperature stars are very common
        // high temperature stars are comparetively rare
        static constexpr const int k_max_temp = 0;
    };

    class RevolvingStarModel final : public sf::Drawable {
    public:
        void set_axis_center          (VectorD pixel_pos);
        void set_maximum_star_distance(double pixels);
        void populate_sky    (Rng &, int star_count);
    private:
        void draw(sf::RenderTarget &, sf::RenderStates) const override;
    };

    void draw(sf::RenderTarget &, sf::RenderStates) const override;

    Sun m_sun;
    Atmosphere m_atmosphere;
    double m_tod = 0., m_day_length = k_default_day_length;
};

// for prairie backdrop
// I need small palettes for flower/vegetation colors
// I'd prefer at three "species" of plants against "grass"

class ForestDecor final : public MapDecorDrawer {
public:
    struct TreeParameters : public PlantTree::CreationParams {
        TreeParameters() {}
        TreeParameters(VectorD location_, std::default_random_engine & rng_):
            CreationParams(PlantTree::generate_params(rng_)),
            location(location_)
        {}
        VectorD location;
    };

    struct FutureTree {
        virtual bool is_ready() const = 0;
        virtual bool is_done() const = 0;
        virtual PlantTree get_tree() = 0;
    };

    struct FutureTreeMaker {
        using TreeParameters = ::ForestDecor::TreeParameters;
        virtual ~FutureTreeMaker() {}

        virtual std::unique_ptr<FutureTree> make_tree(const TreeParameters &) = 0;
    };

    struct Updatable {
        virtual ~Updatable() {}
        virtual void update(double) = 0;
    };

    ForestDecor();

    ForestDecor(ForestDecor &&) = default;
    ~ForestDecor() override;

    ForestDecor & operator = (ForestDecor &&) = default;
#   if 0
    void load_map(const tmap::TiledMap & tmap, MapObjectLoader &) override;
#   endif
    void render_front(sf::RenderTarget &) const override;

    void render_background(sf::RenderTarget &) const override;

    void render_backdrop(sf::RenderTarget &) const override;

    void update(double et) override;

    void set_view_size(int width, int height) override;

private:
    static constexpr const bool k_use_multithreaded_tree_loading = true;

    std::unique_ptr<TempRes> prepare_map_objects(const tmap::TiledMap & tmap, MapObjectLoader &) override;

    void prepare_map(tmap::TiledMap &, std::unique_ptr<TempRes>) override;

    void load_map_vegetation(const tmap::TiledMap &, MapObjectLoader &);
    std::unique_ptr<TempRes> load_map_waterfalls(const tmap::TiledMap &);

    void plant_new_flower(std::default_random_engine &, VectorD, MapObjectLoader &);

    void plant_new_future_tree(std::default_random_engine &, VectorD, MapObjectLoader &);

    std::vector<Flower> m_flowers;
    std::vector<PlantTree> m_trees;
    std::vector<std::pair<std::unique_ptr<FutureTree>, Entity>> m_future_trees;

    std::set<std::shared_ptr<Updatable>> m_updatables;

    std::unique_ptr<FutureTreeMaker> m_tree_maker;

    SolarCycler m_solar_cycler;
};
