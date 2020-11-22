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

SurfaceIter SurfaceIter::operator ++ () {
    auto rv = *this;
    ++m_itr;
    return rv;
}

// ----------------------------------------------------------------------------

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

std::size_t Platform::next_surface(std::size_t idx) const noexcept {
    if (++idx < m_surfaces.size()) return idx;
    return surfaces_cycle() ? 0 : k_no_surface;
}

std::size_t Platform::previous_surface(std::size_t idx) const noexcept {
    if (idx-- > 0) return idx;
    return surfaces_cycle() ? m_surfaces.size() - 1 : k_no_surface;
}

Surface Platform::get_surface(std::size_t idx) const {
    if (has_waypoint_offset())
        return move_surface(m_surfaces.at(idx), waypoint_offset());
    return m_surfaces.at(idx);
}

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

void Platform::move_to(VectorD r)
    { move_by(r - average_location()); }

void Platform::move_by(VectorD delta) {
    for (auto & surf : m_surfaces) {
        surf.a += delta;
        surf.b += delta;
    }
}

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

/* private */ VectorD Platform::offset(const PhysicsComponent * pcomp) const {
    auto rv = has_waypoint_offset() ? waypoint_offset() : VectorD();
    if (pcomp) {
        rv += pcomp->location();
    }
    return rv;
}
