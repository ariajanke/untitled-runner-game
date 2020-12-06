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

#include "GameDriver.hpp"

#include <SFML/Graphics/RenderTarget.hpp>

#include <iostream>

namespace {

inline VectorD box_in(VectorD r, const LineMapLayer & lmap) {
    auto minx = double(k_view_width)*0.5;
    auto miny = double(k_view_height)*0.5;
    auto maxx = double(lmap.width ())*lmap.tile_width () - minx;
    auto maxy = double(lmap.height())*lmap.tile_height() - miny;
    return VectorD(std::floor(std::max(std::min(maxx, r.x), minx)),
                   std::floor(std::max(std::min(maxy, r.y), miny)));
}

ControlEvent to_control_event(const sf::Event &);

} // end of <anonymous> namespace


std::string HudTimePiece::get_seconds() const {
    auto sec_part = int(std::floor(std::fmod(m_total_elapsed_time, 60.)));
    return pad_two(std::to_string(sec_part));
}

std::string HudTimePiece::get_centiseconds() const {
    auto cs_part = int(std::floor(std::fmod(m_total_elapsed_time, 1.)*100.));
    return pad_two(std::to_string(cs_part));
}

std::string HudTimePiece::get_minutes() const {
    auto mins = int(std::floor(m_total_elapsed_time / 60.));
    return pad_two(std::to_string(mins));
}

void HudTimePiece::update_gems_count(int count) {
    auto t = m_gems_count.take_string();
    t.clear();
    t = t + "Gems: " + std::to_string(count);
    m_gems_count.set_text_top_left(VectorD(0, 8), std::move(t));
}

void HudTimePiece::update(double et) {
    m_total_elapsed_time += et;
    auto s = m_timer_text.take_string();
    s.clear();
    s += "Time: " + get_minutes() + ":"
      + get_seconds() + "." + get_centiseconds();
    m_timer_text.set_text_top_left(VectorD(), std::move(s));
}

void HudTimePiece::update_velocity(VectorD r) {
    auto t = m_velocity.take_string();
    t.clear();
    t += "speed " + std::to_string(int(std::round(magnitude(r))))
         + " (" + std::to_string(int(std::round(r.x))) + ", "
         + std::to_string(int(std::round(r.y))) + ")";
    m_velocity.set_text_top_left(VectorD(0, 16), std::move(t));
}

void HudTimePiece::draw(sf::RenderTarget & target, sf::RenderStates states) const {
    target.draw(m_timer_text, states);
    target.draw(m_velocity, states);
    target.draw(m_gems_count, states);
}

void GameDriver::setup(const StartupOptions & opts, const sf::View &) {
    m_rng = std::default_random_engine { std::random_device()() };
    m_tmap.load_from_file(opts.test_map);

    m_lmapnn.load_map_from(m_tmap);
    DriverMapObjectLoader dmol(m_player, m_emanager);
    dmol.load_map_objects(m_tmap.map_objects());
#   if 0
    for (auto & obj : m_tmap.map_objects()) {
        auto load_obj = get_loader_function(obj.type);
        if (load_obj) {
            load_obj(dmol, obj);
        } else {
            // unrecognized game object
        }
    }
#   endif
    setup_systems(CompleteSystemList());
}

void GameDriver::update(double et) {
    for (auto * tsys : m_time_aware_systems) {
        tsys->set_elapsed_time(et);
    }
    m_timer.update(et);
    m_emanager.update_systems();

    m_emanager.process_deletion_requests();

    m_timer.update_velocity(m_player.get<PhysicsComponent>().velocity());
    m_timer.update_gems_count(m_player.get<Collector>().diamond);
}

void GameDriver::render_to(sf::RenderTarget & target) {
    for (const auto & layer : m_tmap) {
        target.draw(*layer);
    }
    for (auto sys : m_render_target_systems) {
        sys->render_to(target);
    }
}

void GameDriver::render_hud_to(sf::RenderTarget & target) {
    target.draw(m_timer);
}

void GameDriver::process_event(const sf::Event & event) {
#   if 0
    using RealDistri = std::uniform_real_distribution<double>;
#   endif
    using IntDistri = std::uniform_int_distribution<int>;
    if (m_player) {
        get_script(m_player)->process_control_event(to_control_event(event));
    }
    switch (event.type) {
    case sf::Event::MouseButtonReleased: {
        auto e = m_emanager.create_new_entity();
        auto & freebody = e.add<PhysicsComponent>().reset_state<FreeBody>();
        freebody.location = m_player.get<PhysicsComponent>().location()
            + VectorD(0, -100);

        add_color_circle(e, random_color(m_rng));
        e.add<Lifetime>().value = 30.;
#       if 0
        e.add<PhysicsDebugDummy>();
#       endif

        auto htype = e.add<Item>().hold_type = Item::simple;//choose_random(m_rng, { Item::run_booster, Item::simple }); //Item::HoldType(IntDistri(0, Item::k_hold_type_count - 1)(m_rng));
        const char * msg = [htype]() {switch (htype) {
        case Item::platform_breaker: return "platform breaker";
        case Item::run_booster     : return "run booster";
        case Item::crate           : return "crate";
        default: return static_cast<const char *>(nullptr);
        }}();
        if (msg) {
            std::cout << msg << std::endl;
        }
        if (htype != Item::jump_booster)
            freebody.velocity = VectorD(0, -100);
#       if 0
        if (htype == Item::crate) {
            e.add<ScriptUPtr>() = std::make_unique<PrintOutLandingsDepartingsScript>();
        }
#       endif
        e.get<PhysicsComponent>().active_layer = m_player.get<PhysicsComponent>().active_layer;
        if (htype == Item::simple) {
             //e.get<PhysicsComponent>().bounce_thershold = 10;
        } else if (htype == Item::jump_booster) {
            e.get<PhysicsComponent>().affected_by_gravity = false;
        }
        }
        break;
    default: break;
    }
}

VectorD GameDriver::camera_position() const {
    if (!m_player) return VectorD();
    const auto & pcomp = m_player.get<PhysicsComponent>();
    const auto & layer = m_lmapnn.get_layer(pcomp.active_layer);
    return box_in(pcomp.location(), layer);
}

template <typename ... Types>
/* private */ void GameDriver::setup_systems(TypeList<>) {
    for (auto & sys_uptr : m_systems) {
        m_emanager.register_system(&*sys_uptr);
    }
    for (auto & lmap_sys : m_map_aware_systems) {
        lmap_sys->assign_map(m_lmapnn);
    }
    for (auto & sys_uptr : m_systems) {
        sys_uptr->setup();
    }
}

template <typename HeadType, typename ... Types>
/* private */ void GameDriver::setup_systems(TypeList<HeadType, Types...>) {
    static_assert (std::is_base_of<System, HeadType>::value, "");
    std::unique_ptr<HeadType> new_sys = std::make_unique<HeadType>();
    if constexpr (std::is_base_of<TimeAware, HeadType>::value) {
        m_time_aware_systems.push_back(&*new_sys);
    }
    if constexpr (std::is_base_of<MapAware, HeadType>::value) {
        m_map_aware_systems.push_back(&*new_sys);
    }
    if constexpr (std::is_base_of<RenderTargetAware, HeadType>::value) {
        m_render_target_systems.push_back(&*new_sys);
    }
#   if 0
    if constexpr (std::is_base_of_v<PhysicsHistoryAware, HeadType>) {
        new_sys->assign_event_sink(m_event_proc);
    }
#   endif
    m_systems.emplace_back(new_sys.release());
    setup_systems<Types...>(TypeList<Types...>());
}

namespace {

ControlEvent to_control_event(const sf::Event & event) {
    auto to_press = [](ControlMove cm)
        { return ControlEvent(PressEvent(cm)); };
    auto to_release = [](ControlMove cm)
        { return ControlEvent(ReleaseEvent(cm)); };
    using ConM = ControlMove;
    switch (event.type) {
    case sf::Event::KeyPressed:
        switch (event.key.code) {
        case sf::Keyboard::A: return to_press(ConM::move_left);
        case sf::Keyboard::D: return to_press(ConM::move_right);
        case sf::Keyboard::W: return to_press(ConM::jump);
        case sf::Keyboard::S: return to_press(ConM::use);
        default: break;
        }
        break;
    case sf::Event::KeyReleased:
        switch (event.key.code) {
        case sf::Keyboard::A: return to_release(ConM::move_left);
        case sf::Keyboard::D: return to_release(ConM::move_right);
        case sf::Keyboard::W: return to_release(ConM::jump);
        case sf::Keyboard::S: return to_release(ConM::use);
        default: break;
        }
        break;
    default: break;
    }
    return ControlEvent();
}

} // end of <anonymous> namespace
