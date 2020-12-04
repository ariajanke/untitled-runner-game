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

#include "EnvironmentCollisionSystem.hpp"
#include "LineTrackerPhysics.hpp"
#include "FreeBodyPhysics.hpp"

#include <iostream>

#include <cassert>

/* static */ void EnvironmentCollisionSystem::run_tests() {
#   if 0
    static auto test_refl = []
        (double ax, double ay, double bx, double by,
         VectorD approach, VectorD expected_res)
    {
        LineSegment seg(ax, ay, bx, by);
        auto refl = reflect_approach(seg, approach);
        return magnitude(refl - expected_res) < k_error;
    };
#       if 1
    assert(test_refl(0, 0, 1, 0, VectorD(0.5, -0.5), VectorD( 0.5,  0.5)));
    assert(test_refl(0, 0, 0, 1, VectorD(0.5, -0.5), VectorD(-0.5, -0.5)));
    assert(test_refl(0, 0, 1, 1, VectorD(0, -1), VectorD(-1, 0)));
    assert(test_refl(0, 0, 1, 1, VectorD(1, 0), VectorD(0, 1)));
#       endif
    assert(test_refl(0, 0, 2, -1, VectorD(1, -2), VectorD(2.2, 0.4)));
#   endif
}

EnvColParams::EnvColParams
    (PhysicsComponent & pcomp, const LineMap & map_, const PlayerControl * pcon,
     const PlatformsCont & platforms_):
    bounce_thershold (pcomp.bounce_thershold),
    layer            (pcomp.active_layer),
    map              (map_),
    platforms        (platforms_)
{
    if (!pcon) {
        acting_will = false;
    } else {
        acting_will = (direction_of(*pcon) != 0.);
    }
}

// ----------------------------------------------------------------------------

/* private */ void EnvironmentCollisionSystem::update(const ContainerView & cont) {
    m_platforms.clear();
    for (auto & e : cont) {
        if (e.has<Platform>()) m_platforms.push_back(e);
    }
    for (auto & e : cont) { update(e); }
}

/* private */ void EnvironmentCollisionSystem::update(Entity & e) {
    if (!e.has<PhysicsComponent>()) return;
    auto & pcomp = e.get<PhysicsComponent>();
    EnvColParams ecp(pcomp, line_map(), e.ptr<PlayerControl>(), m_platforms);
    ecp.set_owner(e);
    auto old_id = pcomp.state_type_id();
    switch (pcomp.state_type_id()) {
    case k_freebody_state: {
        const auto & fb =  pcomp.state_as<FreeBody>();
        handle_freebody_physics(ecp, fb.location + fb.velocity*elapsed_time());
        }
        break;
    case k_tracker_state:
        handle_tracker_physhics(ecp, elapsed_time());
        break;
    default: return;
    }
    if (old_id != pcomp.state_type_id() && ecp.should_log_debug()) {
        switch (pcomp.state_type_id()) {
        case k_freebody_state:
            std::cout << "Transformed into freebody." << std::endl;
            break;
        case k_tracker_state :
            std::cout << "Transformed into line tracker." << std::endl;
            break;
        default: break;
        }
    }
}
