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

#include "SystemsMisc.hpp"
#if 0
class EnvColEvents {
public:
    void set_entity(Entity e) { this_entity = e; }

    void record_landing(EntityRef eref, VectorD velocity) {
        if (Entity e = Entity(eref)) record_landing(e.ptr<ScriptEvents>(), velocity);
    }
    void record_departing(EntityRef eref) {
        if (Entity e = Entity(eref)) record_departing(e.ptr<ScriptEvents>());
    }
private:
    void record_landing  (ScriptEvents * events, VectorD velocity) {
        if (events) events->record_landing(velocity, this_entity);
    }
    /**
     *  @param events comes from the entity with the script events (e.g. the
     *                left/right scale part)
     */
    void record_departing(ScriptEvents * events) {
        if (events) events->record_departing(this_entity);
    }
    EntityRef this_entity;
};
#endif

struct EnvColStateMask {

    void set_owner(Entity e) {
        m_owner = e;
        m_pcomp = e.ptr<PhysicsComponent>();
    }

    template <typename T>
    bool state_is_type() const {
        return m_owner.get<PhysicsComponent>().state_is_type<T>();
    }

    void set_landing(const LineTracker & tracker, const SurfaceRef & surfref, VectorD velo) {
        auto & new_tracker = m_pcomp->reset_state<LineTracker>();
        new_tracker.set_owner(m_owner);
        new_tracker = tracker;
        new_tracker.set_surface_ref(surfref, velo);
    }

    void set_transfer(const LineTracker & tracker) {
        auto & new_tracker = m_pcomp->reset_state<LineTracker>();
        new_tracker.set_owner(m_owner);
        new_tracker = tracker;
    }

    void set_freebody(const FreeBody & freebody) {
        m_pcomp->reset_state<FreeBody>() = freebody;
    }

    template <typename T>
    T & state_as() { return m_pcomp->state_as<T>(); }

    template <typename T>
    const T & state_as() const { return m_pcomp->state_as<T>(); }

    template <typename T>
    T * state_as_ptr() { return m_pcomp->state_ptr<T>(); }

    template <typename T>
    const T * state_as_ptr() const { return m_pcomp->state_ptr<T>(); }

    int state_type_id() const { return m_pcomp->state_type_id(); }
private:
    Entity m_owner;
    PhysicsComponent * m_pcomp = nullptr;
};

struct EnvColParams final : public EnvColStateMask {
    using PlatformsCont = std::vector<Entity>;
    EnvColParams() {}
    EnvColParams(PhysicsComponent &, const LineMap &, const PlayerControl *,
                 const PlatformsCont &);

          bool            acting_will      = false;
          double          bounce_thershold = std::numeric_limits<double>::infinity();
          Layer         & layer            = get_null_reference<Layer>();
    const LineMap       & map              = get_null_reference<LineMap>();
    const PlatformsCont & platforms        = get_null_reference<PlatformsCont>();
};

class EnvironmentCollisionSystem final :
    public System, public MapAware, public TimeAware
{
public:
    static void run_tests();

private:
    void update(const ContainerView & cont) override;

    void update(Entity & e);

    std::vector<Entity> m_platforms;
};
