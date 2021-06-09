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

#include <tmap/TiledMap.hpp>

#include <SFML/Window/Event.hpp>

#include "Components.hpp"
#include "Systems.hpp"
#include "GraphicalEffects.hpp"
#include "GraphicsDrawer.hpp"

#include "maps/Maps.hpp"
#include "maps/MapObjectLoader.hpp"

#include <algorithm>
#include <iostream>

static const constexpr int k_view_width  = 426;
static const constexpr int k_view_height = 240;
static const constexpr bool k_map_object_loader_rng_is_deterministic = false;

namespace tmap {

class TileLayer;

}

using CompleteSystemList = cul::TypeList<
    EnvironmentCollisionSystem,
    LifetimeSystem,
    SnakeSystem,
    PlayerControlSystem,
    AnimatorSystem,
    DrawSystem,
    TriggerBoxSystem,
    TriggerBoxOccupancySystem,
    GravityUpdateSystem,
    ExtremePositionsControlSystem,
    PlatformDrawer,
    WaypointPositionSystem,
    PlatformMovementSystem,
    HoldItemSystem,
    PlatformBreakingSystem,
    CratePositionUpdateSystem,
    RecallBoundsSystem,
    FallOffSystem,
    ScriptUpdateSystem
>;

class DriverMapObjectLoader final : public MapObjectLoader {
public:
    using MapObjectContainer = tmap::MapObject::MapObjectContainer;
    DriverMapObjectLoader(Entity & player_, EntityManager & ent_man_):
        m_player(player_), m_ent_man(ent_man_)
    {
        if constexpr (k_map_object_loader_rng_is_deterministic) {
            static constexpr const auto k_seed = 0xDEADBEEF;
            m_rng = std::default_random_engine{ k_seed };
        } else {
            m_rng = std::default_random_engine{ std::random_device()() };
        }
    }

private:
    Entity create_entity() override {
        return m_ent_man.create_new_entity();
    }

    Entity create_named_entity_for_object() override {
        //message_assert("", m_current_object);
        auto rv = m_ent_man.create_new_entity();
        if (m_current_object->name.empty()) {
            throw std::runtime_error("Cannot name entity, its object has no name in the map data.");
        }
        bool success = m_name_entity_map.insert( std::make_pair(m_current_object->name, rv) ).second;
        if (!success) {
            throw std::runtime_error("");
        }
        return rv;
    }

    Entity find_named_entity(const std::string & name) const override {
        auto itr = m_name_entity_map.find(name);
        if (itr != m_name_entity_map.end()) return itr->second;
        throw std::runtime_error("Entity named \"" + name + "\", was the name forgotten or was the requirement entities not loaded yet (need to patch entity requirements code)?");
    }

    void set_player(Entity e) override {
        if (m_player) { m_player.request_deletion(); }
        m_player = e;
    }

    std::default_random_engine & get_rng() override
        { return m_rng; }

    const tmap::MapObject * find_map_object(const std::string & name) const override {
        auto itr = m_name_obj_map.find(name);
        if (itr == m_name_obj_map.end()) return nullptr;
        return itr->second;
    }

public:
    void load_map_objects(const MapObjectContainer & cont) {
        auto order = get_map_load_order(cont, &m_name_obj_map);
        for (const auto * obj : order) {
            m_current_object = obj;
            get_loader_function(obj->type)(*this, *obj);
        }
    }

    void load_tile_objects(const tmap::TileLayer &);

private:
    std::map<std::string, const tmap::MapObject *> m_name_obj_map;
    std::map<std::string, Entity> m_name_entity_map;

    const tmap::MapObject * m_current_object = nullptr;
    Entity & m_player;
    EntityManager & m_ent_man;
    std::default_random_engine m_rng;
};

class FpsCounter {
public:
    static constexpr const bool k_have_std_dev = true;
    void update(double et) {
        ++m_count_this_frame;
        push_frame_et(et);
        if ( (m_total_et += et) > 1. ) {
            m_fps = m_count_this_frame;
            m_count_this_frame = 0;
            m_total_et = std::fmod(m_total_et, 1.);
            update_second_et_std_dev();
        }
    }

    int fps() const noexcept { return m_fps; }

    std::enable_if_t<k_have_std_dev, double> std_dev() const noexcept
        { return m_et_std_dev; }

    std::enable_if_t<k_have_std_dev, double> avg() const noexcept
        { return m_et_avg;; }

private:
    void push_frame_et(double et) {
        if constexpr (k_have_std_dev) {
            m_ets.push_back(et);
        }
    }

    void update_second_et_std_dev() {
        if constexpr (k_have_std_dev) {
            m_et_avg = 0.;
            for (auto x : m_ets) m_et_avg += x;
            m_et_avg /= double(m_ets.size());

            m_et_std_dev = 0.;
            for (auto x : m_ets) {
                m_et_std_dev += (x - m_et_avg)*(x - m_et_avg);
            }
            m_et_std_dev = std::sqrt(m_et_std_dev);

            m_ets.clear();
        }
    }


    int m_fps = 0, m_count_this_frame = 0;
    double m_total_et = 0.;

    double m_et_std_dev = 0., m_et_avg = 0.;
    std::vector<double> m_ets;
};

class HudTimePiece final : public sf::Drawable {
public:
    HudTimePiece() {
        m_timer_text.load_internal_font();
        m_gems_count = m_velocity = m_timer_text;
    }
    void update_gems_count(int);
    void update(double et);
    void update_velocity(VectorD);

    void set_debug_line(int, const std::string &);

private:
    void draw(sf::RenderTarget &, sf::RenderStates) const override;

    static std::string pad_two(std::string && str)
        { return (str.length() < 2) ? "0" + std::move(str) : std::move(str); }

    std::string get_seconds() const;

    std::string get_centiseconds() const;

    std::string get_minutes() const;

    double m_total_elapsed_time = 0.;
    TextDrawer m_gems_count;
    TextDrawer m_timer_text;
    TextDrawer m_velocity;

    std::vector<TextDrawer> m_debug_lines;

    FpsCounter m_fps_counter;
};

class TopSpdTracker {
public:
    void update(VectorD current_velocity, HudTimePiece &);
    void clear_record();

private:
    VectorD m_top_vel;
};

inline void TopSpdTracker::update(VectorD current_velocity, HudTimePiece & hud) {
    if (magnitude(current_velocity) > magnitude(m_top_vel)) {
        m_top_vel = current_velocity;
        hud.set_debug_line(1, "Top Velocity: "
                           + std::to_string(round_to<int>(magnitude(m_top_vel))) + " ("
                           + std::to_string(round_to<int>(m_top_vel.x)) + ", "
                           + std::to_string(round_to<int>(m_top_vel.y)) +")");
    } else if (m_top_vel == VectorD()) {
        hud.set_debug_line(1, "Top Velocity: 0 (0, 0)");
    }
}

inline void TopSpdTracker::clear_record() { m_top_vel = VectorD(); }

class LocationTracker {
public:
    void update(VectorD current_location, HudTimePiece &);
    void clear_record();

private:
    struct RectBounds {
        RectBounds() {}
        RectBounds(double low_x_, double low_y_, double high_x_, double high_y_):
            low_x(low_x_), low_y(low_y_), high_x(high_x_), high_y(high_y_)
        {}
        bool are_same(const RectBounds & rhs) const {
            auto is_eq = std::equal_to<double>();
            return    is_eq(low_x , rhs.low_x ) && is_eq(low_y , rhs.low_y )
                   && is_eq(high_x, rhs.high_x) && is_eq(high_y, rhs.high_y);
        }
        bool operator == (const RectBounds & rhs) const { return  are_same(rhs); }
        bool operator != (const RectBounds & rhs) const { return !are_same(rhs); }
        double low_x, low_y, high_x, high_y;
    };
    static RectBounds make_default_rect();
    void update_hud(HudTimePiece &) const;

    RectBounds m_bounds = make_default_rect();
};

inline void LocationTracker::update(VectorD current_location, HudTimePiece & hud) {
    using std::min;
    using std::max;
    RectBounds new_bounds(min(m_bounds.low_x , current_location.x),
                          min(m_bounds.low_y , current_location.y),
                          max(m_bounds.high_x, current_location.x),
                          max(m_bounds.high_y, current_location.y));
    if (new_bounds != m_bounds) {
        m_bounds = new_bounds;
        update_hud(hud);
    }
}

inline void LocationTracker::clear_record() {
    m_bounds = make_default_rect();
}

inline /* private static */ LocationTracker::RectBounds LocationTracker::make_default_rect() {
    return RectBounds(k_inf, k_inf, -k_inf, -k_inf);
}

inline /* private */ void LocationTracker::update_hud(HudTimePiece & hud) const {
    static auto to_string = [](double x)
        { return std::to_string(round_to<int>(x)); };
    hud.set_debug_line(2, "Extreme Bounds : (l r) ("
                       + to_string(m_bounds.low_x) + " " + to_string(m_bounds.high_x)
                       + ") (d u) (" + to_string(m_bounds.low_y) + " "
                       + to_string(m_bounds.high_y) + ")");
}

class GameDriver final {
public:

    void setup(const StartupOptions &, const sf::View &);
    void update(double);
    void render_to(sf::RenderTarget &);
    void render_hud_to(sf::RenderTarget &);
    void process_event(const sf::Event &);
    VectorD camera_position() const;

#   if 1
    const Entity & get_player() const { return m_player; }
#   endif

private:
    template <typename ... Types>
    void setup_systems(cul::TypeList<>);

    template <typename HeadType, typename ... Types>
    void setup_systems(cul::TypeList<HeadType, Types...>);

    template <typename ... Types>
    void setup_systems() { setup_systems(cul::TypeList<Types...>()); }

    tmap::TiledMap m_tmap;
    EntityManager m_emanager;

    LineMap m_lmapnn;

    std::vector<std::unique_ptr<System>> m_systems;
    std::vector<TimeAware *> m_time_aware_systems;
    std::vector<MapAware *> m_map_aware_systems;

    Entity m_player;

    std::default_random_engine m_rng;

    HudTimePiece m_timer;
    GraphicsDrawer m_graphics;

    // info only
    TopSpdTracker m_vtrkr;
    LocationTracker m_ltrkr;
};
