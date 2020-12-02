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

#include "SystemsDefs.hpp"

class PlayerControlSystem final : public System, public TimeAware {
    static constexpr const double k_acceleration        = 125.;
    static constexpr const double k_breaking_boost      = 3.;
    static constexpr const double k_max_voluntary_speed = 400;
    static constexpr const double k_booster_factor      = 2.5;
    static constexpr const double k_jump_speed          = 333.;

    void update(const ContainerView & view) {
        for (auto e : view) update(e);
    }

    void update(Entity e);

    void handle_freebody_running(FreeBody &, const PlayerControl &);

    void handle_tracker_running(LineTracker &, const PlayerControl &, EntityRef carrying);

    static void handle_tracker_jumping(PhysicsComponent &, const LineTracker &, PlayerControl &);
};

class LifetimeSystem final : public System, public TimeAware {
    void update(const ContainerView & view) {
        for (auto & e : view) {
            if (!e.has<Lifetime>()) continue;
            auto & lt = e.get<Lifetime>().value;
            lt -= elapsed_time();
            if (lt < 0.) {
                e.request_deletion();
            }
        }
    }
};

class SnakeSystem final : public System, public TimeAware {
    void update(const ContainerView & view);
    void update(Entity & e) const;
    static sf::Color instance_color(const Snake & snake);
};

class ExtremePositionsControlSystem final : public System, public MapAware {
    void update(const ContainerView & cont) override
        { for (auto & e : cont) { update(e); } }

    void update(Entity & e);
};

class GravityUpdateSystem final : public System, public TimeAware {
    void update(const ContainerView & cont) override
        { for (auto e : cont) { update(e); } }

    void update(Entity e);
};

class DiamondCollectSparkles {
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

class ItemCollisionSystem final :
    public System, public MapAware, public RenderTargetAware, public TimeAware
{
public:
    void update(const ContainerView & cont) override;

    void check_item_collection();

    void check_item_collection(Entity collector, Entity item);

    void render_to(sf::RenderTarget & target) override {
        m_sparkles.render_to(target);
    }
#   if 0
    void check_item_grabs();

    void check_item_grabs(Entity & grabber, Entity & grabable);
#   endif
private:
    DiamondCollectSparkles m_sparkles;
    std::vector<Entity> m_items;
    std::vector<Entity> m_collectors;
};

class LauncherSystem final : public System, public MapAware {
    void update(const ContainerView &) override;

    static void do_bounce(PhysicsComponent &, const Launcher &);

    std::vector<Entity> m_bouncy_surfaces;
    std::vector<Entity> m_bouncables;
};


class PlatformWaypointSystem final : public System, public TimeAware {
    void update(const ContainerView & view) override {
        for (auto e : view) {
            if (!e.has<Platform>()) continue;
            auto & plat = e.get<Platform>();
            if (plat.waypoint_number == Platform::k_no_waypoint ||
                !plat.waypoints) continue;
            if (plat.waypoints->empty()) continue;
            auto seg_len = segment_length(plat.current_waypoint_segment());
            if (seg_len < k_error) continue;
            plat.position += plat.speed*elapsed_time() / seg_len;
            auto next_waypt = Platform::k_no_waypoint;
            if (plat.position < 0.) {
                next_waypt = plat.previous_waypoint();
            } else if (plat.position > 1.) {
                next_waypt = plat.next_waypoint();
            } else {
                continue;
            }
            // no change implies non-cycling waypoints
            if (next_waypt == plat.waypoint_number) {
                plat.position = std::max(1., std::min(0., plat.position));
            }
            // other cases, we are cycling
            else if (plat.position < 0.) {
                plat.position = 1.;
            } else if (plat.position > 1.) {
                plat.position = 0.;
            }
            plat.waypoint_number = next_waypt;
        }
    }
};

class HoldItemSystem final : public System {

    void update(const ContainerView & view) override;

    static void check_to_hold(Entity holder, Entity holdable);
    static void check_release(Entity held);

    static void clear_player_con_releases(Entity e)
        { e.get<PlayerControl>().releasing = false; }

    std::vector<Entity> m_holders  ;
    std::vector<Entity> m_holdables;

};

class PlatformBreakingSystem final : public System {
    void update(const ContainerView & view) override {
        m_platforms.clear();
        m_items.clear();
        for (auto e : view) {
            if (e.has<Platform>()) m_platforms.push_back(e);
            if (e.has<Item>() && e.has<PhysicsComponent>()) {
                if (e.get<Item>().hold_type == Item::platform_breaker)
                    m_items.push_back(e);
            }
        }
        for (auto plat : m_platforms) {
            for (auto item : m_items) {
                if (plat == item) continue;
                update(plat, item);
            }
        }
    }

    void update(Entity platform_e, Entity item_e) {
        VectorD low(k_inf, k_inf), high(-k_inf, -k_inf);
        auto & plat = platform_e.get<Platform>();
        for (const auto & plat : plat.surface_view(platform_e.ptr<PhysicsComponent>())) {
            low .x = std::min(plat.a.x, low .x);
            low .x = std::min(plat.b.x, low .x);
            low .y = std::min(plat.a.y, low .y);
            low .y = std::min(plat.b.y, low .y);
            high.x = std::max(plat.a.x, high.x);
            high.x = std::max(plat.b.x, high.x);
            high.y = std::max(plat.a.y, high.y);
            high.y = std::max(plat.b.y, high.y);
        }
        Rect rect(low.x, low.y, high.x - low.x, high.y - low.y);
        static constexpr const double k_min_size = 10.;
        if (magnitude(rect.width) < k_min_size) {
            rect.width = k_min_size;
            rect.left -= k_min_size*0.5;
        }
        if (magnitude(rect.height) < k_min_size) {
            rect.height = k_min_size;
            rect.top -= k_min_size*0.5;
        }
        if (!rect.contains(item_e.get<PhysicsComponent>().location())) return;
        platform_e.request_deletion();
    }

    std::vector<Entity> m_platforms;
    std::vector<Entity> m_items    ;
};
#if 1
class CratePositionUpdateSystem final : public System {
    void update(const ContainerView & view) override {
        for (auto e : view) {
            if (auto * itm = e.ptr<Item>()) {
                if (itm->hold_type == Item::crate)
                    { update(e); }
            }
        }
    }
    void update(Entity e) {
        auto & pcomp = e.get<PhysicsComponent>();
        if (pcomp.state_is_type<HeldState>()) {
            if (e.has<Platform>()) e.remove<Platform>();
        } else if (!e.has<Platform>()) {
            Surface surf;
            surf.a.x = -30.;
            surf.b.x =  30.;
            surf.a.y = surf.b.y = -60.;
            e.add<Platform>().set_surfaces(std::vector<Surface>({ surf }));
        }
    }
};
#endif
class FallOffSystem final : public System {
    void update(const ContainerView & view) override {
        for (auto e : view) {
            auto * tracker = get_tracker(e);
            if (!tracker) continue;
            auto eref = tracker->surface_ref().attached_entity();
            if (!eref) continue;

            if (eref.is_requesting_deletion()) {
                ; // do nothing
            } else {
                if (Entity(eref).has<Platform>()) continue;
            }
        }
    }
};

class RecallBoundsSystem final : public System, public TimeAware {
    void update(const ContainerView & view) override {
        for (auto e : view) {
           update(e);
        }
    }

    void update(Entity e) const {
        if (!e.has<ReturnPoint>() || !e.has<PhysicsComponent>()) return;
        const auto & pcomp = e.get<PhysicsComponent>();
        auto & rt_point = e.get<ReturnPoint>();
        if (   rect_contains(rt_point.recall_bounds, pcomp.location())
            || pcomp.state_is_type<HeldState>())
        {
            rt_point.recall_time = rt_point.recall_max_time;
            return;
        }
        if ((rt_point.recall_time -= elapsed_time()) <= 0.) {
            rt_point.recall_time = 0.;
            auto & pcomp = e.get<PhysicsComponent>();
            auto & fb = pcomp.reset_state<FreeBody>();
            fb.velocity = VectorD();
            fb.location = center_of(Entity(rt_point.ref).get<PhysicsComponent>().state_as<Rect>());
            // post disappear and reappear effects
        }
    }
};

