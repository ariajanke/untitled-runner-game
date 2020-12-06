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

#include <common/TestSuite.hpp>

#include <iostream>

#include <cassert>

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

Surface Platform::get_surface(std::size_t idx) const
    { return move_surface(m_surfaces.at(idx), m_offset); }

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

// ----------------------------------------------------------------------------

/* static */ const InterpolativePosition::SegPair InterpolativePosition::
    k_no_segment_pair = []()
{
    SegPair pair;
    pair.target = pair.source = std::size_t(-1);
    return pair;
}();

double InterpolativePosition::move_position(double x) {
    if (!is_real(x)) {
        throw InvArg("InterpolativePosition::move_position: x must be a real number.");
    }
    if (m_behavior == k_idle) return x;

    InterpolativePosition old = *this;
    double rv;
    if (m_position + x > 1.) {
        assert(x > 0.);
        if (next_point() == k_no_point_back) {
            rv = x - (1. - m_position);
            m_position = 1.; // go ahead and max out
        } else {
            m_current_point = next_point();
            rv = (m_position + x) - 1.;
            m_position = 0.;
        }
    } else if (m_position + x < 0.) {
        assert(x < 0.);
        if (previous_point() == k_no_point_back) {
            rv = x + m_position;
            m_position = 0.;
        } else {
            m_current_point = previous_point();
            rv = m_position + x;
            m_position = 1.;
        }
    } else {
        rv = 0.;
        m_position += x;
    }
    check_invarients();
    // rv should be the same sign as x
    if (rv*x < 0.) {
        old.move_position(x);
    }
    assert(rv*x >= 0.);
    return rv;
}

void InterpolativePosition::set_whole_position(double x) {
    if (!is_real(x) || x < 0. || x >= double(m_segment_count)) {
        throw InvArg("InterpolativePosition::set_whole_position: x must be a "
                     "real non-negative number, not greater than "
                     + std::to_string(m_segment_count) + ".");
    }
    m_position      = std::remainder(x, 1.);
    m_current_point = SegUInt(std::floor(x));

    if (     m_position < k_error) m_position = 0.;
    if (1. - m_position < k_error) m_position = 1.;
    check_invarients();
}

void InterpolativePosition::set_position(double x) {
    if (!is_real(x)) {
        throw InvArg("InterpolativePosition::set_position: x must be a real number.");
    } else if (x < 0. || x > 1.) {
        throw InvArg("InterpolativePosition::set_position: x must be in [1 0].");
    }
    m_position = x;
    check_invarients();
}

double InterpolativePosition::position() const noexcept
    { return m_position; }

void InterpolativePosition::set_speed(double x) {
    if (!is_real(x)) {
        throw InvArg("InterpolativePosition::set_speed: x must be a real number.");
    }
    m_speed = x;
    check_invarients();
}

double InterpolativePosition::speed() const noexcept
    { return m_speed; }

void InterpolativePosition::set_behavior(Behavior behavior_) {
    if (behavior_ == k_toward_destination) {
        throw InvArg("InterpolativePosition::set_behavior: this behavior must "
                     "be set with the \"target_point\" method.");
    }
    m_behavior = behavior_;
    check_invarients();
}

InterpolativePosition::Behavior InterpolativePosition::behavior() const noexcept
    { return m_behavior; }

void InterpolativePosition::target_point(std::size_t x) {
    auto t = verify_fit(x, "InterpolativePosition::target_point");
    if (t > m_segment_count) {
        std::string range_ = "(only 0)";
        if (m_segment_count > 1) {
            range_ = "[0 " + std::to_string(m_segment_count - 1) + "]";
        }
        throw InvArg("InterpolativePosition::target_point: cannot target point: "
                     + std::to_string(t) + " possible range is: " + range_       );
    }
    m_behavior    = k_toward_destination;
    m_destination = t;
}

std::size_t InterpolativePosition::targeted_point() const noexcept {
    return (m_behavior == k_toward_destination) ? m_destination : k_no_point;
}

InterpolativePosition::SegPair InterpolativePosition::current_segment() const {
    auto mk_seg = [](SegUInt t, SegUInt s) {
        SegPair rv;
        rv.target = std::size_t(t);
        rv.source = std::size_t(s);
        return rv;
    };
    auto verify_pair = [this](SegPair pair) {
        assert(pair.source <= m_segment_count);
        assert(pair.target <= m_segment_count);
        return pair;
    };
    auto target = next_point() == k_no_point_back ? m_current_point : next_point();
    return verify_pair(mk_seg(m_current_point, target));
}

void InterpolativePosition::set_segment_count(std::size_t x) {
    auto t = verify_fit(x, "InterpolativePosition::set_segment_count");
    m_position      = 0.;
    m_current_point = 0;

    if (m_destination > t) { m_destination = 0; }
    m_segment_count = t;
}

void InterpolativePosition::set_segment_source(std::size_t x) {
    auto t = verify_fit(x, "InterpolativePosition::set_segment_source");
    if (t > m_segment_count) {
        throw InvArg("InterpolativePosition::set_segment_source: source must "
                     "not exceed the number points, which is "
                     + std::to_string(m_segment_count) + ".");
    }
    m_current_point = t;
}

/* static */ void InterpolativePosition::run_tests() {
    ts::TestSuite suite;
    suite.start_series("InterpolativePosition tests");
    suite.test([]() {
        ;
        return ts::test(false);
    });
}

/* private */ InterpolativePosition::SegUInt
    InterpolativePosition::previous_point() const noexcept
{
    switch (m_behavior) {
    case k_foreward:
        if (m_current_point == 0) return k_no_point_back;
        return m_current_point - 1;
    case k_cycles:
        if (m_current_point == 0) return m_segment_count;
        return m_current_point - 1;
    case k_idle: return k_no_point_back;
    case k_toward_destination:
        if (m_current_point == m_destination) {
            // I'm not sure what's the best value for this...
            return m_current_point;
        } else if (m_current_point > m_destination) {
            return (m_current_point == m_segment_count) ? 0 : m_current_point + 1;
        } else {
            return (m_current_point == 0) ? m_segment_count : m_current_point - 1;
        }
    }
    std::cerr << "InterpolativePosition::previous_point: bad branch" << std::endl;
    std::terminate();
}

/* private */ InterpolativePosition::SegUInt
    InterpolativePosition::next_point() const noexcept
{
    switch (m_behavior) {
    case k_foreward:
        if (m_current_point == m_segment_count) return k_no_point_back;
        return m_current_point + 1;
    case k_cycles  :
        if (m_current_point == m_segment_count) return 0;
        return m_current_point + 1;
    case k_idle    : return k_no_point_back;
    case k_toward_destination:
        if (m_current_point == m_destination) {
            return k_no_point_back;
        } else if (m_current_point > m_destination) {
            return (m_current_point == 0) ? m_segment_count : m_current_point - 1;
        } else {
            return (m_current_point == m_segment_count) ? 0 : m_current_point + 1;
        }
    }
    std::cerr << "InterpolativePosition::next_point: bad branch" << std::endl;
    std::terminate();
}

/* private */ void InterpolativePosition::check_invarients() const {
    assert(m_position >= 0. && m_position <= 1.);
}

/* private static */ InterpolativePosition::SegUInt InterpolativePosition::
    verify_fit(std::size_t x, const char * caller)
{
    if (x > std::numeric_limits<SegUInt>::max()) {
        throw InvArg(std::string(caller)
                     + ": value cannot fit in underlying data type.");
    }
    return SegUInt(x);
}

#if 0
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
    if (!waypoints) return VectorD();
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
    switch (m_behavior) {
    case k_idle     : return waypoint_number;
    case k_forewards: return increment_index(false, waypoint_number);
    case k_cycles   : return increment_index(true , waypoint_number);
    case k_toward_destination:
        if (waypoint_number == m_dest_waypoint)
            return waypoint_number;
        else if (waypoint_number < m_dest_waypoint)
            return increment_index(false, waypoint_number);
        else
            return decrement_index(false, waypoint_number);
    }
    throw BadBranchException();
}

std::size_t Waypoints::previous_waypoint() const {
    switch (m_behavior) {
    case k_idle     : return waypoint_number;
    case k_forewards: return decrement_index(false, waypoint_number);
    case k_cycles   : return decrement_index(true , waypoint_number);
    case k_toward_destination:
        if (waypoint_number == m_dest_waypoint)
            return waypoint_number;
        else if (waypoint_number < m_dest_waypoint)
            return decrement_index(false, waypoint_number);
        else
            return increment_index(false, waypoint_number);
    }
    throw BadBranchException();
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
#endif
