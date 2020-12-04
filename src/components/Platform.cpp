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

#include "Platform.hpp"

namespace {

using RtError = std::runtime_error;
using InvArg  = std::invalid_argument;

} // end of <anonymous> namespace

SurfaceIter SurfaceIter::operator ++ () {
    auto rv = *this;
    ++m_itr;
    return rv;
}

// ----------------------------------------------------------------------------
#if 0
VectorD Platform::waypoint_offset() const {
    auto seg = current_waypoint_segment();
    return seg.a + (seg.b - seg.a)*position;
}

bool Platform::has_waypoint_offset() const noexcept
    { return waypoints ? waypoints->size() > 1 : false; }

LineSegment Platform::current_waypoint_segment() const {
    if (!has_waypoint_offset()) {
        throw std::runtime_error("Platform has no line segment that "
            "describes its travel between two waypoints as there are fewer "
            "than two waypoints.");
    }
    return LineSegment((*waypoints)[waypoint_number], (*waypoints)[next_waypoint()]);
}

SurfaceView Platform::surface_view_relative() const {
    return SurfaceView(m_surfaces.begin(), m_surfaces.end(), offset(nullptr));
}

SurfaceView Platform::surface_view(const PhysicsComponent * pcomp) const {
    return SurfaceView(m_surfaces.begin(), m_surfaces.end(), offset(pcomp));
}
#endif

SurfaceView Platform::surface_view() const {
    return SurfaceView(m_surfaces.begin(), m_surfaces.end(), m_offset);
}

std::size_t Platform::next_surface(std::size_t idx) const noexcept {
    if (++idx < m_surfaces.size()) return idx;
    return surfaces_cycle() ? 0 : k_no_surface;
}

std::size_t Platform::previous_surface(std::size_t idx) const noexcept {
    if (idx-- > 0) return idx;
    return surfaces_cycle() ? m_surfaces.size() - 1 : k_no_surface;
}
#if 0
Surface Platform::get_surface(std::size_t idx) const {
    if (has_waypoint_offset())
        return move_surface(m_surfaces.at(idx), waypoint_offset());
    return m_surfaces.at(idx);
}
#endif
#if 0
Surface Platform::get_surface(std::size_t idx, const Waypoints * waypts) const {
    if (waypts) {
        if (waypts->has_waypoint_offset()) {
            return move_surface(m_surfaces.at(idx), waypts->waypoint_offset());
        }
    }
    return m_surfaces.at(idx);
}
#endif

Surface Platform::get_surface(std::size_t idx) const
    { return move_surface(m_surfaces.at(idx), m_offset); }

#if 0
std::size_t Platform::next_waypoint() const {
    if (waypoint_number + 1 == waypoints->size()) {
        return cycle_waypoints ? 0 : waypoint_number;
    }
    return waypoint_number + 1;
}

std::size_t Platform::previous_waypoint() const {
    if (waypoint_number == 0) {
        return cycle_waypoints ? waypoints->size() - 1 : waypoint_number;
    }
    return waypoint_number - 1;
}
#endif
#if 0
void Platform::move_to(VectorD r)
    { move_by(r - average_location()); }

void Platform::move_by(VectorD delta) {
    for (auto & surf : m_surfaces) {
        surf.a += delta;
        surf.b += delta;
    }
}
#endif
/* private */ bool Platform::surfaces_cycle() const noexcept {
    if (m_surfaces.size() < 2) return false;
    return are_very_close(m_surfaces.front().a, m_surfaces.back().b);
}

/* private */ VectorD Platform::average_location() const {
    VectorD avg_pt;
    double count = 0.;
    for (const auto & surf : m_surfaces) {
        avg_pt += surf.a + surf.b;
        count  += 2.;
    }
    return avg_pt*(1. / count);
}
#if 0
/* private */ VectorD Platform::offset(const PhysicsComponent * pcomp) const {
    auto rv = has_waypoint_offset() ? waypoint_offset() : VectorD();
    if (pcomp) {
        rv += pcomp->location();
    }
    return rv;
}
#endif
// ----------------------------------------------------------------------------

Waypoints::Behavior Waypoints::behavior() const
    { return m_behavior; }

void Waypoints::set_behavior(Behavior behavior_) {
    switch (behavior_) {
    case k_idle: case k_forewards: case k_cycles:
        m_dest_waypoint = k_no_waypoint;
        m_behavior = behavior_;
        break;
    case k_toward_destination:
        throw InvArg("Waypoints::set_behavior: this behavior may only be set "
                     "from: set_destination_waypoint method");
    default:
        throw InvArg("Waypoints::set_behavior: invalid value for enumeration type.");
    }
}

std::size_t Waypoints::destination_waypoint() const
    { return m_dest_waypoint; }

void Waypoints::set_destination_waypoint(std::size_t dest) {
    static constexpr auto k_no_waypoints_msg =
        "Waypoints::set_destination_waypoint: cannot set a destination for "
        "waypoint component which no waypoints have been set.";

    if (!waypoints) throw InvArg(k_no_waypoints_msg);
    if (dest > waypoints->size()) throw InvArg(k_no_waypoints_msg);

    m_dest_waypoint = dest;
    m_behavior = k_toward_destination;
}

VectorD Waypoints::waypoint_offset() const {
    auto seg = current_waypoint_segment();
    return seg.a + (seg.b - seg.a)*position;
}

bool Waypoints::has_waypoint_offset() const noexcept
    { return waypoints ? waypoints->size() > 1 : false; }

LineSegment Waypoints::current_waypoint_segment() const {
    if (!has_waypoint_offset()) {
        throw std::runtime_error("Platform has no line segment that "
            "describes its travel between two waypoints as there are fewer "
            "than two waypoints.");
    }
    return LineSegment((*waypoints)[waypoint_number], (*waypoints)[next_waypoint()]);
}

std::size_t Waypoints::next_waypoint() const {
#   if 0
    //bool on_last = (waypoint_number + 1 == waypoints->size());
#   endif
    switch (m_behavior) {
    case k_idle     : return waypoint_number;
    case k_forewards: return increment_index(false, waypoint_number);// return on_last ? waypoint_number : waypoint_number + 1;
    case k_cycles   : return increment_index(true , waypoint_number);// on_last ?               0 : waypoint_number + 1;
    case k_toward_destination:
        if (waypoint_number == m_dest_waypoint)
            return waypoint_number;
        else if (waypoint_number < m_dest_waypoint)
            return increment_index(false, waypoint_number);
        else
            return decrement_index(false, waypoint_number);
    }
    throw BadBranchException();
#   if 0
    if (waypoint_number + 1 == waypoints->size()) {
        return cycle_waypoints ? 0 : waypoint_number;
    }
    return waypoint_number + 1;
#   endif
}

std::size_t Waypoints::previous_waypoint() const {
#   if 0
    bool on_last = (waypoint_number == 0);
#   endif
    switch (m_behavior) {
    case k_idle     : return waypoint_number;
    case k_forewards: return decrement_index(false, waypoint_number);// on_last ? waypoint_number       : waypoint_number - 1;
    case k_cycles   : return decrement_index(true , waypoint_number);// on_last ? waypoints->size() - 1 : waypoint_number - 1;
    case k_toward_destination:
        if (waypoint_number == m_dest_waypoint)
            return waypoint_number;
        else if (waypoint_number < m_dest_waypoint)
            return decrement_index(false, waypoint_number);
        else
            return increment_index(false, waypoint_number);
    }
    throw BadBranchException();
#   if 0
    if (waypoint_number == 0) {
        return cycle_waypoints ? waypoints->size() - 1 : waypoint_number;
    }
    return waypoint_number - 1;
#   endif
}

/* private */ std::size_t Waypoints::increment_index
    (bool cycle, std::size_t idx) const
{
    if (!waypoints) {
        throw RtError(std::string("Waypoints::increment_index")
                      + k_no_waypoints_assigned_msg);
    }
    if (idx + 1 == waypoints->size()) {
        return cycle ? 0 : idx;
    }
    return idx + 1;
}

/* private */ std::size_t Waypoints::decrement_index
    (bool cycle, std::size_t idx) const
{
    if (!waypoints) {
        throw RtError(std::string("Waypoints::decrement_index")
                      + k_no_waypoints_assigned_msg);
    }
    if (idx == 0) {
        return cycle ? waypoints->size() - 1 : idx;
    }
    return idx - 1;
}

#if 0
// ----------------------------------------------------------------------------

SurfaceView make_surface_view(const Platform & plat)
    { return make_surface_view(plat, nullptr, nullptr); }

SurfaceView make_surface_view(const Platform & plat, const PhysicsComponent * pcomp)
    { return make_surface_view(plat, pcomp, nullptr); }

SurfaceView make_surface_view(const Platform & plat, const Waypoints * waypts)
    { return make_surface_view(plat, nullptr, waypts); }

SurfaceView make_surface_view
    (const Platform & plat, const PhysicsComponent * pcomp, const Waypoints * waypts)
{
    VectorD offset;
    if (pcomp) {
        offset += pcomp->location();
    }
    if (waypts) {
        if (waypts->has_waypoint_offset()) {
            offset += waypts->waypoint_offset();
        }
    }
    return SurfaceView(plat.m_surfaces.begin(), plat.m_surfaces.end(), offset);
}
#endif
