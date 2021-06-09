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

#include "SystemsDefs.hpp"

class PlayerControlSystem final : public System, public TimeAware {
    static constexpr const double k_acceleration        = 125.;
    static constexpr const double k_breaking_boost      = 3.;
    static constexpr const double k_max_voluntary_speed = 400;
    static constexpr const double k_booster_factor      = 2.5;
    static constexpr const double k_jump_speed          = 333.;
    // no more jumping to the moon >:c
    static constexpr const double k_max_jump_speed      = 500.;

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

class TriggerBoxSystem final : public System, public GraphicsAware {
public:
    static bool is_subject(const Entity & e);

private:
    template <typename ... Types>
    using Tuple = std::tuple<Types...>;

    using SubjectContainer = std::vector<Tuple<Entity, TriggerBoxSubjectHistory &>>;
    using CheckPoint       = TriggerBox::Checkpoint;
    using Launcher         = TriggerBox::Launcher;
    using TargetedLauncher = TriggerBox::TargetedLauncher;

    void update(const ContainerView &) override;

    void on_graphics_assigned() override {
        m_item_checker.assign_graphics(graphics());
        m_checkpoints.assign_graphics(graphics());
    }

    class BaseChecker {
    public:
        virtual ~BaseChecker() {}
        void add(Entity e) { m_trespassees.push_back(e); }
        void do_checks(const SubjectContainer &);
        void assign_graphics(GraphicsBase & graphics)
            { m_graphics = &graphics; }

    protected:
        virtual void handle_trespass(Entity trespassee, Entity trespasser) const = 0;
        virtual void adjust_collision(const Entity & collector, VectorD & offset, Rect & bounds) const = 0;
        GraphicsBase & graphics() const { return *m_graphics; }
    private:
        static Rect get_rect(const Entity & e)
            { return e.get<PhysicsComponent>().state_as<Rect>(); }
        static bool is_entering_box(VectorD old, VectorD new_, const Rect & rect) {
            if (old == TriggerBoxSubjectHistory::k_no_location) return false;
            return    !is_contained_in(old, rect)/* rect.contains(old)*/
                   && line_crosses_rectangle(rect, old, new_);
        }
        // cleared every frame
        std::vector<Entity> m_trespassees;
        GraphicsBase * m_graphics = nullptr;
    };

    class CheckPointChecker final : public BaseChecker {
        void handle_trespass(Entity checkpoint, Entity e) const override;
        void adjust_collision(const Entity &, VectorD &, Rect &) const override {}
    };

    class ItemChecker final : public BaseChecker {
        void handle_trespass(Entity collectable, Entity e) const override;
        void adjust_collision(const Entity & collector, VectorD & offset, Rect &) const override;
    };

    class LaunchCommonBase : public BaseChecker {
    protected:
        static void do_set(VectorD launch_vel, PhysicsComponent &, PlayerControl *);
        static VectorD get_detached_position(const LineTracker &);
    };

    class LauncherChecker final : public LaunchCommonBase {
        static constexpr const double k_max_boost = 900.;
        void handle_trespass(Entity launch_e, Entity e) const override;
        void adjust_collision(const Entity &, VectorD &, Rect &) const override {}

        static void do_boost (VectorD launch_vel, PhysicsComponent &);
        static void do_launch(VectorD launch_vel, PhysicsComponent &);
    };

    class TargetedLauncherChecker final : public LaunchCommonBase {
        void handle_trespass(Entity launch_e, Entity e) const override;
        void adjust_collision(const Entity &, VectorD &, Rect &) const override {}
    };

    class ScriptChecker final : public BaseChecker {
        void handle_trespass(Entity launch_e, Entity e) const override;
        void adjust_collision(const Entity &, VectorD &, Rect &) const override {}
    };

    SubjectContainer m_subjects;
    ItemChecker m_item_checker;
    LauncherChecker m_launchers;
    CheckPointChecker m_checkpoints;
    TargetedLauncherChecker m_target_launchers;
    ScriptChecker m_scripts;
};

class TriggerBoxOccupancySystem final : public System, public TimeAware {
    void update(const ContainerView & view) override {
        m_subjects.clear();
        m_targets .clear();
        for (auto & e : view) {
            std::vector<Entity> * vec = nullptr;
            if (TriggerBoxSystem::is_subject(e)) {
                vec = &m_subjects;
            } else if (e.has<TriggerBox>() && e.has<PhysicsComponent>() && get_script(e)) {
                if (e.get<PhysicsComponent>().state_is_type<Rect>())
                    vec = &m_targets;
            }
            if (!vec) continue;
            vec->push_back(e);
        }

        for (auto subject : m_subjects) {
        for (auto target  : m_targets ) {
            if (subject == target) continue;
            do_check(target, subject, elapsed_time());
        }}
    }

    static void do_check(Entity target, Entity subject, double et) {
        auto & target_script = *get_script(target);
        auto target_rect = target.get<PhysicsComponent>().state_as<Rect>();
#       if 0
        if (target_rect.contains(subject.get<PhysicsComponent>().location())) {
#       endif
        if (is_contained_in(subject.get<PhysicsComponent>().location(), target_rect)) {
            target_script.on_box_occupancy(target, subject, et);
            return;
        }

        static const auto k_no_location = TriggerBoxSubjectHistory::k_no_location;
        auto last_loc = get_last_location(subject);
        if (last_loc == k_no_location) return;
#       if 0
        if (target_rect.contains(last_loc)) {
#       endif
        if (is_contained_in(last_loc, target_rect)) {
            target_script.on_box_occupancy(target, subject, et);
        }
    }

    static VectorD get_last_location(const Entity & e) {
        using History = TriggerBoxSubjectHistory;
        return e.has<History>() ? e.get<History>().last_location() : History::k_no_location;
    }

    std::vector<Entity> m_subjects;
    std::vector<Entity> m_targets ;
};

class WaypointPositionSystem final : public System, public TimeAware {
    void update(const ContainerView & view) override {
        for (auto e : view) {
            if (should_skip(e)) continue;
            update(e.get<Waypoints>().points(), e.get<InterpolativePosition>(), elapsed_time());
        }
    }

    static void update
        (const Waypoints::Container & waypts, InterpolativePosition & intpos, double et)
    {
        if (et < k_error) return;
        auto seg_len = segment_length(get_waypoint_segment(waypts, intpos));
        // can't move anymore, we must be at a stopping point
        if (seg_len < k_error) return;

        auto delta = (intpos.speed() / seg_len)*et;
        auto rem   = intpos.move_position(delta);

        if (rem / delta > 1.) {
            throw std::runtime_error("something's wrong");
        }
        update(waypts, intpos, et * (rem / delta));
    }

    static bool should_skip(Entity e) {
        if (!e.has<Waypoints>() || !e.has<InterpolativePosition>()) return true;
        return false;
    }
};

class PlatformMovementSystem final : public System {
    // waypoints position -> platform positions
    // physics component
    void update(const ContainerView & view) {
        for (auto e : view) {
            if (!e.has<Platform>()) continue;
            update(e);
        }
    }

    void update(Entity e) {
        VectorD offset;
        const auto * waypts = e.ptr<Waypoints>();
        const auto * intpos = e.ptr<InterpolativePosition>();
        if (waypts && intpos) {
            offset += get_waypoint_location(*waypts, *intpos); //waypts->waypoint_offset();
        }
        if (const auto * pcomp = e.ptr<PhysicsComponent>()) {
            offset += pcomp->location();
        }
        e.get<Platform>().set_offset(offset);
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
        for (const auto & plat : plat.surface_view()) {
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
#       if 0
        if (!rect.contains(item_e.get<PhysicsComponent>().location())) return;
#       endif
        if (!is_contained_in(item_e.get<PhysicsComponent>().location(), rect)) return;
        platform_e.request_deletion();
    }

    std::vector<Entity> m_platforms;
    std::vector<Entity> m_items    ;
};

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

class ScriptUpdateSystem final : public System, public TimeAware {
    void update(const ContainerView & view) override {
        for (auto e : view) {
            if (!should_skip(e)) continue;
            e.get<ScriptUPtr>()->on_update(e, elapsed_time());
        }
    }

    static bool should_skip(Entity e) {
        return e.has<ScriptUPtr>();
    }
};
