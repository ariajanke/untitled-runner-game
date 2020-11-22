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

#include "SystemsDefs.hpp"

#include "../maps/Maps.hpp"

// gotta have unique filenames for all translation units

const LineMapLayer & MapAware::get_map_layer(const Layer & layer) {
    return m_lmap->get_layer(layer);
}

/* vtable anchor */ RenderTargetAware::~RenderTargetAware() {}
#if 0
// ----------------------------------------------------------------------------

void PhysicsHistoryEventProcessor::record_landing
    (Entity plat_ent, VectorD vel, EntityRef hitter)
{
    Landing l;
    l.velocity = vel;
    l.hitter = hitter;
    m_events.emplace_back(std::make_pair(plat_ent, Event(l)));
}

void PhysicsHistoryEventProcessor::record_departing
    (Entity plat_ent, EntityRef departer)
{
    Departing d;
    d.departer = departer;
    m_events.emplace_back(std::make_pair(plat_ent, Event(d)));
}

void PhysicsHistoryEventProcessor::process_events() {
    for (const auto & [ent, event] : m_events) {
        switch (event.type_id()) {
        case Event::GetTypeId<Landing  >::k_value: {
            const auto & l = event.as<Landing>();
            ent.get<ScriptUPtr>()->on_landing(ent, l.velocity, l.hitter);
            }
            break;
        case Event::GetTypeId<Departing>::k_value:
            ent.get<ScriptUPtr>()->on_departing(ent, event.as<Departing>().departer);
            break;
        default: throw ImpossibleBranchException();
        }
    }
    m_events.clear();
}

PhysicsHistoryWatcher::PhysicsHistoryWatcher
    (Entity entity, PhysicsHistoryEventProcessor * sink):
    m_entity(entity),
    m_sink(sink)
{
    update_history(entity.get<PhysicsComponent>());
}

PhysicsHistoryWatcher::~PhysicsHistoryWatcher() noexcept(false) {
    // don't bother if we're in the middle of an exception
    if (std::uncaught_exceptions() > 0) return;
    // platform specific stuff
    [this]() {

        const auto & pcomp = m_entity.get<PhysicsComponent>();
        EntityRef new_attached;
        EntityRef old_attached = pcomp.history.surface_ref.attached_entity();
        if (pcomp.state_is_type<LineTracker>()) {
            new_attached = pcomp.state_as<LineTracker>().surface_ref.attached_entity();
        }
        if (new_attached == old_attached) return;

        if (Entity plat = Entity(new_attached)) {
            if (get_script(plat)) {
                auto new_vel = velocity_of(pcomp);
                m_sink->record_landing(plat, new_vel, m_entity);
            }
        }

        if (Entity plat = Entity(old_attached)) {
            if (get_script(plat))
                m_sink->record_departing(plat, m_entity);
        }
    } ();
}

/* vtable anchor */ PhysicsHistoryAware::~PhysicsHistoryAware() {}
#endif
