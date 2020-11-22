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
#include "../maps/SurfaceRef.hpp"

#include <common/MultiType.hpp>
#if 0
struct PhysicsEventBase {
    virtual ~PhysicsEventBase();
    void depart_from(EntityRef);
    void land_on    (EntityRef, VectorD);

    static PhysicsEventBase & null_instance();
protected:
    virtual void depart_from_(EntityRef) = 0;
    virtual void land_on_    (EntityRef, VectorD) = 0;
};
#endif
struct FreeBody {
    VectorD location;
    VectorD velocity;
};
#if 0
class LineTrackerLandingPriv;
#endif
class LineTracker {
public:
#   if 0
    // this strategy seems to have worked before
    friend class LineTrackerLandingPriv;
#   endif
    LineTracker() {}
    LineTracker(const LineTracker &);
    LineTracker(LineTracker &&);

    LineTracker & operator = (const LineTracker &);
    LineTracker & operator = (LineTracker &&);

    ~LineTracker();

    // how am I going to identify the normal?
    // the normal determines what's "outside"
    bool inverted_normal = false;
    // presently the units are in number of presently attached segments
    // rather than pixels
    double position = 0.;
    double speed = 0.;

    SurfaceRef surface_ref() const { return m_surface_ref; }
#   if 0
    void set_surface_ref(SurfaceRef ref, VectorD impact_vel /* non-optional for now */);
    void set_surface_ref(EntityRef owner, SurfaceRef ref, VectorD impact_vel /* non-optional for now */);
    void set_owner(EntityRef owner) { m_owning_entity = owner; }

    void assign_events(PhysicsEventBase & peb) { m_events = &peb; }
#   endif
    void set_owner(EntityRef owner) { m_owning_entity = owner; }

    void set_surface_ref(const SurfaceRef & ref, VectorD impact_vel = VectorD());
private:
#   if 0
    void set_surface_ref(SurfaceRef ref, VectorD impact_vel /* non-optional for now */);
#   endif
    void swap(LineTracker &);

    SurfaceRef m_surface_ref;
    EntityRef m_owning_entity;
#   if 0
    PhysicsEventBase * m_events = &PhysicsEventBase::null_instance();
#   endif
};

class ItemPickerPriv;
struct HeldState {
    friend class ItemPickerPriv;
    // describes what to do to the holder when released
    using ReactFunc = void (*)(EntityRef);
    void set_release_func(ReactFunc func) { m_release_func = func; }
    EntityRef holder() const { return m_holder; }
private:
    ReactFunc m_release_func = [](EntityRef) {};
    EntityRef m_holder;
};

using PhysicsState = MultiType<LineTracker, FreeBody, Rect, HeldState>;

const constexpr int k_tracker_state   = PhysicsState::GetTypeId<LineTracker>::k_value;
const constexpr int k_freebody_state  = PhysicsState::GetTypeId<FreeBody   >::k_value;
const constexpr int k_rectangle_state = PhysicsState::GetTypeId<Rect       >::k_value;
const constexpr int k_held_state      = PhysicsState::GetTypeId<HeldState  >::k_value;

struct PhysicsHistory {
    VectorD    location;
    VectorD    velocity;
    SurfaceRef surface_ref;
};

struct PhysicsComponent {

#   if 0
    VectorD previous_location;
#   endif
    PhysicsHistory history;
    double bounce_thershold = std::numeric_limits<double>::infinity();

    Layer active_layer = Layer::foreground;
#   if 1
    bool affected_by_gravity = true;
#   endif

    template <typename Type>
    Type & reset_state() { return m_state.reset<Type>(); }

    bool state_is_valid() const { return m_state.is_valid(); }

    template <typename Type>
    bool state_is_type() const { return m_state.is_type<Type>(); }

    template <typename Type>
    Type & state_as() { return m_state.as<Type>(); }

    template <typename Type>
    const Type & state_as() const { return m_state.as<Type>(); }

    template <typename Type>
    Type * state_ptr() { return m_state.as_pointer<Type>(); }

    template <typename Type>
    const Type * state_ptr() const { return m_state.as_pointer<Type>(); }

    int state_type_id() const noexcept { return m_state.type_id(); }
#   if 0
    void set_state(const PhysicsState & pstate) { m_state = pstate; }
#   endif
    VectorD location() const;
    VectorD velocity() const;
    VectorD normal  () const;
private:
    PhysicsState m_state;
};

[[deprecated]] void update_history(PhysicsComponent &);

VectorD location_of(const LineTracker &);
#if 0
VectorD location_of(const PhysicsState &);

inline VectorD location_of(const PhysicsComponent & pcomp)
    { return location_of(pcomp.state); }

VectorD velocity_of(const PhysicsState &);

inline VectorD velocity_of(const PhysicsComponent & pcomp)
    { return velocity_of(pcomp.state); }
#endif
VectorD normal_for(const LineSegment &, bool inverted_normal);

inline VectorD normal_for(const LineTracker & tracker)
    { return normal_for(*tracker.surface_ref(), tracker.inverted_normal); }
#if 0
VectorD normal_for(const PhysicsState &);

inline VectorD normal_for(const PhysicsComponent & pcomp)
    { return normal_for(pcomp.state); }
#endif
