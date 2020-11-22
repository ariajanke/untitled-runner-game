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

#include "PhysicsComponent.hpp"

#include "ComponentsComplete.hpp"
#if 0
/* vtable anchor */ PhysicsEventBase::~PhysicsEventBase() {}

void PhysicsEventBase::depart_from(EntityRef ref) {
    if (!ref) return;
    depart_from_(ref);
}

void PhysicsEventBase::land_on(EntityRef ref, VectorD impact_vel) {
    if (!ref) return;
    land_on_(ref, impact_vel);
}

/* static */ PhysicsEventBase & PhysicsEventBase::null_instance() {
    struct NullPhysicsEvent final : public PhysicsEventBase {
        void depart_from_(EntityRef) {}
        void land_on_    (EntityRef, VectorD) {}
    };
    static NullPhysicsEvent instance;
    return instance;
}

// ----------------------------------------------------------------------------
#endif
LineTracker::LineTracker(const LineTracker & rhs):
    inverted_normal(rhs.inverted_normal),
    position       (rhs.position       ),
    speed          (rhs.speed          )
{
    set_surface_ref(rhs.surface_ref()/*, VectorD()*/);
}

LineTracker::LineTracker(LineTracker && rhs) {
    swap(rhs);
}

LineTracker & LineTracker::operator = (const LineTracker & rhs) {
    LineTracker temp(rhs);
    swap(temp);
    return *this;
}

LineTracker & LineTracker::operator = (LineTracker && rhs) {
    swap(rhs);
    return *this;
}

LineTracker::~LineTracker() {
    if (!m_owning_entity) return;
    if (Entity e = Entity(m_surface_ref.attached_entity())) {
        if (auto * script = e.ptr<ScriptUPtr>()) {
            (**script).on_departing(e, m_owning_entity);
        }
    }
#   if 0
    m_events->depart_from(m_surface_ref.attached_entity());
#   endif
}
#if 0
void LineTracker::set_surface_ref(SurfaceRef ref, VectorD impact_vel) {
    if (!m_owning_entity) {
        throw std::runtime_error("LineTracker::set_surface_ref: owner must be set first.");
    }
    if (Entity e = Entity(m_surface_ref.attached_entity())) {
        if (auto * script = e.ptr<ScriptUPtr>()) {
            (**script).on_departing(e, m_owning_entity);
        }
    }
    if (Entity e = Entity(ref.attached_entity())) {
        if (auto * script = e.ptr<ScriptUPtr>()) {
            (**script).on_landing(e, impact_vel, m_owning_entity);
        }
    }
    m_surface_ref = ref;
}
#endif
#if 0
void LineTracker::set_surface_ref(EntityRef owner, SurfaceRef ref, VectorD impact_vel) {
    m_owning_entity = owner;
    set_surface_ref(ref, impact_vel);
}
#endif

void LineTracker::set_surface_ref(const SurfaceRef & ref, VectorD impact_vel) {
    if (m_owning_entity && m_surface_ref != ref) {
        if (Entity e = Entity(m_surface_ref.attached_entity())) {
            if (auto * script = e.ptr<ScriptUPtr>()) {
                (**script).on_departing(e, m_owning_entity);
            }
        }
        if (Entity e = Entity(ref.attached_entity())) {
            if (auto * script = e.ptr<ScriptUPtr>()) {
                (**script).on_landing(e, impact_vel, m_owning_entity);
            }
        }
    }
    m_surface_ref = ref;
}

void LineTracker::swap(LineTracker & other) {
    std::swap(inverted_normal, other.inverted_normal);
    std::swap(position       , other.position       );
    std::swap(speed          , other.speed          );

    auto temp = other.surface_ref();
    other.set_surface_ref(surface_ref());
          set_surface_ref(temp         );
}

// ----------------------------------------------------------------------------

VectorD PhysicsComponent::location() const {
    switch (m_state.type_id()) {
    case k_freebody_state:
        return m_state.as<FreeBody>().location;
    case k_tracker_state:
        return location_of(m_state.as<LineTracker>());
    case k_held_state: {
        auto & hstate = m_state.as<HeldState>();
        // need to be careful here... especially with MT possiblities
        Entity e { hstate.holder() };
        if (e.has<HeadOffset>())
            return hand_point_of<HeadOffset, PhysicsComponent>(e);
        else
            return e.get<PhysicsComponent>().location();
        }
    case k_rectangle_state:
        return center_of(m_state.as<Rect>());
    default: break;
    }

    if (!m_state.is_valid()) {
        throw std::invalid_argument("location_of: PhysicsState does not store "
                                    "anything, and therefore no location is possible.");
    }
    throw ImpossibleBranchException();
}

VectorD PhysicsComponent::velocity() const {
    switch (m_state.type_id()) {
    case k_freebody_state:
        return m_state.as<FreeBody>().velocity;
    case k_tracker_state:
        return velocity_along( m_state.as<LineTracker>().speed,
                              *m_state.as<LineTracker>().surface_ref());
    case k_held_state: case k_rectangle_state: return VectorD();
    default: throw ImpossibleBranchException();
    }
}

VectorD PhysicsComponent::normal() const {
    if (auto * tracker = m_state.as_pointer<LineTracker>()) {
        return normal_for(*tracker);
    } else {
        return -normalize(k_gravity);
    }
}

// ----------------------------------------------------------------------------

VectorD normal_for(const LineSegment & segment, bool inverted_normal) {
    double perp = (inverted_normal ? -1. : 1.)*(k_pi*0.5);
    return normalize(rotate_vector(segment.b - segment.a, perp));
}

#if 0
VectorD normal_for(const PhysicsState & pstate) {
    if (auto * tracker = pstate.as_pointer<LineTracker>()) {
        return normal_for(*tracker);
    } else {
        return -normalize(k_gravity);
    }
}
#endif
void update_history(PhysicsComponent &) {
#   if 0
    pcomp.history.location = pcomp.location();// location_of(pcomp);
    pcomp.history.velocity = pcomp.velocity();// velocity_of(pcomp);
    if (auto * tracker = pcomp.state.as_pointer<LineTracker>()) {
        pcomp.history.surface_ref = tracker->surface_ref();
    } else {
        pcomp.history.surface_ref = SurfaceRef();
    }
#   endif
}

VectorD location_of(const LineTracker & tracker) {
    return location_along(tracker.position, *tracker.surface_ref());
}
#if 0
VectorD location_of(const PhysicsState & pstate) {
    switch (pstate.type_id()) {
    case k_freebody_state:
        return pstate.as<FreeBody>().location;
    case k_tracker_state:
        return location_of(pstate.as<LineTracker>());
    case k_held_state: {
        auto & hstate = pstate.as<HeldState>();
        // need to be careful here... especially with MT possiblities
        Entity e { hstate.holder() };
        if (e.has<HeadOffset>())
            return hand_point_of<HeadOffset, PhysicsComponent>(e);
        else
            return location_of(e.get<PhysicsComponent>());
        }
    case k_rectangle_state:
        return center_of(pstate.as<Rect>());
    default: break;
    }

    if (!pstate.is_valid()) {
        throw std::invalid_argument("location_of: PhysicsState does not store "
                                    "anything, and therefore no location is possible.");
    }
    throw ImpossibleBranchException();
}

VectorD velocity_of(const PhysicsState & state) {
    switch (state.type_id()) {
    case k_freebody_state:
        return state.as<FreeBody>().velocity;
    case k_tracker_state:
        return velocity_along( state.as<LineTracker>().speed,
                              *state.as<LineTracker>().surface_ref());
    case k_held_state: case k_rectangle_state: return VectorD();
    default: throw ImpossibleBranchException();
    }
}
#endif
