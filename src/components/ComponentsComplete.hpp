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
#include "DisplayFrame.hpp"
#include "ComponentsMisc.hpp"
#include "PhysicsComponent.hpp"
#include "Platform.hpp"

struct PhysicsDebugDummy {};

using EntityManager = ecs::EntityManager<
    PhysicsComponent, // essentially refers to other components
                      // (which I hate, because this means coupling AND
                      //  complex behaviors in a component)
    Lifetime, Snake, PlayerControl, DisplayFrame, Item,
    Launcher, LauncherSubjectHistory,
    Collector, HeadOffset, Platform,
    InterpolativePosition, Waypoints,
    PhysicsDebugDummy,
    ReturnPoint, ScriptUPtr //ScriptEvents
>;

using Entity = EntityManager::EntityType;

// ------------ Helpers that need Entity to be a complete type ----------------

void land_tracker(LineTracker &, Entity, const SurfaceRef &, VectorD);
void transfer_to(LineTracker &, const SurfaceRef &, VectorD);
class LineTrackerLandingPriv {
    friend void land_tracker(LineTracker &, Entity, const SurfaceRef &, VectorD);
    friend void transfer_to(LineTracker &, const SurfaceRef &, VectorD);
    static void land_tracker_
        (LineTracker & tracker, Entity owner, const SurfaceRef & ref, VectorD vel)
    {
        tracker.set_owner(owner);
        tracker.set_surface_ref(ref, vel);
    }
    static void transfer_to_(LineTracker & tracker, const SurfaceRef & ref, VectorD vel)
    { tracker.set_surface_ref(ref, vel); }
};

inline void land_tracker
    (LineTracker & tracker, Entity owner, const SurfaceRef & entity_hit, VectorD impact_vel)
    { LineTrackerLandingPriv::land_tracker_(tracker, owner, entity_hit, impact_vel); }

inline void transfer_to(LineTracker & tracker, const SurfaceRef & entity_hit, VectorD impact_vel)
    { LineTrackerLandingPriv::transfer_to_(tracker, entity_hit, impact_vel); }

// this is perhaps bad design, what components are being accessed?
void pick_up_item(Entity holder, Entity holdable);
void release_held_item(Entity);
void detach_from_holder(Entity);
class ItemPickerPriv {
    friend void pick_up_item(Entity, Entity);
    friend void release_held_item(Entity);
    friend void detach_from_holder(Entity);
    static void pick_up_item(Entity holder, Entity holdable) {
        holdable.get<PhysicsComponent>().state_as<HeldState>().m_holder = holder;
        holder.get<Collector>().m_held_object = holdable;
    }
    static void release_item(Entity holder, Entity holdable) {
        auto & hstate = holdable.get<PhysicsComponent>().state_as<HeldState>();
        auto & collector = holder.get<Collector>();
        if (hstate.m_holder != holder || collector.m_held_object != holdable) {
            throw std::runtime_error("bad link");
        }

        hstate.m_release_func(holder);
        hstate   .m_release_func = [](EntityRef) {};
        hstate   .m_holder       = EntityRef();
        collector.m_held_object  = EntityRef();
    }
};

inline void pick_up_item(Entity holder, Entity holdable) {
    ItemPickerPriv::pick_up_item(holder, holdable);
}

inline void release_held_item(Entity holder) {
    ItemPickerPriv::release_item(holder, Entity(holder.get<Collector>().held_object()));
}

inline void detach_from_holder(Entity held) {
    ItemPickerPriv::release_item(
        Entity(held.get<PhysicsComponent>().state_as<HeldState>().holder()), held);
}

inline EntityRef get_holder(const PhysicsComponent & pcomp) {
    return pcomp.state_is_type<HeldState>() ? pcomp.state_as<HeldState>().holder() : EntityRef();
}

template <typename ... Types>
VectorD hand_point_of(Entity e) {
    using ProvList = TypeList<Types...>;
    static_assert(ProvList::template HasType<HeadOffset>::k_value &&
                  ProvList::template HasType<PhysicsComponent>::k_value,
            "The list of given types must contain the types that are accessed "
            "by this function.");
    return (e.get<PhysicsComponent>().location()) +
           (e.get<PhysicsComponent>().normal())*magnitude(e.get<HeadOffset>())*0.5;
}

void detach_from_holder(Entity);

LineTracker * get_tracker(Entity e);

FreeBody * get_freebody(Entity e);

Rect * get_rectangle(Entity e);

inline Layer & get_layer(Entity & e)
    { return e.get<PhysicsComponent>().active_layer; }

inline const Layer & get_layer(const Entity & e)
    { return e.get<PhysicsComponent>().active_layer; }

void add_color_circle(Entity, sf::Color);

struct EntityHasher {
    std::size_t operator () (const Entity & e) const { return e.hash(); }
};

inline Script * get_script(Entity e) {
    if (auto * uptrptr = e.ptr<ScriptUPtr>()) return uptrptr->get();
    return nullptr;
}

// ----------------------------------------------------------------------------

// yup... guess this project has a script type too!
class Script {
public:
    virtual ~Script() {}
    virtual void process_control_event(const ControlEvent &);
    /**
     *  @param hit_velocity
     *  @param other
     *  @note further alterations to physics states will *not* trigger further
     *        event calls. Doing so may result in infinite recursion!
     */
    virtual void on_landing(Entity, VectorD hit_velocity, EntityRef other) = 0;
    virtual void on_departing(Entity, EntityRef other) = 0;

    virtual void on_update(Entity, double) {}
};

class PlayerScript final : public Script {
public:
    PlayerScript(Entity);
private:
    void process_control_event(const ControlEvent &) override;
    void on_departing(Entity, EntityRef) override;
    void on_landing(Entity, VectorD, EntityRef) override;

    // player is a pretty privileged script
    Entity m_player;
};

class ScalePivotScript final {
public:
    static void add_pivot_script_to(Entity pivot, Entity left, Entity right);
    ScalePivotScript(Entity pivot, Entity left, Entity right);
    void land_left (VectorD velo, EntityRef);
    void land_right(VectorD velo, EntityRef);
    void leave_left (EntityRef);
    void leave_right(EntityRef);

private:
    void print_out_weights() const;
    void update_balance();
    int m_left_weight = 0, m_right_weight = 0, m_lr_balance = 0;

    Entity m_pivot, m_left, m_right;
};

class BasketScript final : public Script {
public:
    void set_wall(Entity e) { m_basket_wall = e; }
private:
    void process_control_event(const ControlEvent &) override {}
    void on_departing(Entity, EntityRef) override;
    void on_landing(Entity, VectorD, EntityRef) override;
    void on_update(Entity, double) override;

    static bool is_simple_item(const Entity &);
    void change_weight(int delta, InterpolativePosition &);

    Entity m_basket_wall;
    int m_held_weight = 0;
    InterpolativePosition::SegPair m_seg;
};

#if 0
class PrintOutLandingsDepartingsScript final : public Script {
    void on_departing(Entity, EntityRef) override;
    void on_landing(Entity, VectorD, EntityRef) override;
};
#endif
