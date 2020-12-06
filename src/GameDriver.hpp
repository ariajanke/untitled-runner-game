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

#include "maps/Maps.hpp"
#include "maps/MapObjectLoader.hpp"

#include <algorithm>

static const constexpr int k_view_width  = 426;
static const constexpr int k_view_height = 240;
static const constexpr bool k_map_object_loader_rng_is_deterministic = false;

using CompleteSystemList = TypeList<
    EnvironmentCollisionSystem,
    LifetimeSystem,
    SnakeSystem,
    PlayerControlSystem,
    AnimatorSystem,
    DrawSystem,
    ItemCollisionSystem,
    LauncherSystem,
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
        for (const auto & obj : cont) {
            if (obj.name.empty()) continue;
            m_name_obj_map[obj.name] = &obj;
        }
        for (const auto & obj : cont) {
            auto load_obj = get_loader_function(obj.type);
            if (load_obj) {
                load_obj(*this, obj);
            } else {
                // unrecognized game object
            }
        }
        m_name_obj_map.clear();
    }

private:
    std::map<std::string, const tmap::MapObject *> m_name_obj_map;
    Entity & m_player;
    EntityManager & m_ent_man;
    std::default_random_engine m_rng;
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
};


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
    void setup_systems(TypeList<>);

    template <typename HeadType, typename ... Types>
    void setup_systems(TypeList<HeadType, Types...>);

    template <typename ... Types>
    void setup_systems() { setup_systems(TypeList<Types...>()); }

    tmap::TiledMap m_tmap;
    EntityManager m_emanager;

    LineMap m_lmapnn;

    std::vector<std::unique_ptr<System>> m_systems;
    std::vector<TimeAware *> m_time_aware_systems;
    std::vector<MapAware *> m_map_aware_systems;
    std::vector<RenderTargetAware *> m_render_target_systems;

    Entity m_player;

    std::default_random_engine m_rng;

    HudTimePiece m_timer;
};
