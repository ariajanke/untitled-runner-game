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

double InterpolativePosition::move_position(double x) {
    if (!is_real(x)) {
        throw InvArg("InterpolativePosition::move_position: x must be a real number.");
    }
    if (m_behavior == k_idle) return x;

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
    assert(rv*x >= 0.);
    return rv;
}

void InterpolativePosition::set_whole_position(double x) {
    if (!is_real(x) || x < 0. || x > double(m_segment_count)) {
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
    if (t > point_count()) {
        std::string range_ = "(only 0)";
        if (point_count() > 1) {
            range_ = "[0 " + std::to_string(point_count()) + "]";
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
    auto mk_seg = [](SegUInt s, SegUInt t) {
        SegPair rv;
        rv.target = std::size_t(t);
        rv.source = std::size_t(s);
        return rv;
    };
    auto verify_pair = [this](SegPair pair) {
        assert(pair.source <= point_count());
        assert(pair.target <= point_count());
        return pair;
    };
    auto target = next_point() == k_no_point_back ? m_current_point : next_point();
    return verify_pair(mk_seg(m_current_point, target));
}

void InterpolativePosition::set_point_count(std::size_t x) {
    auto t = verify_fit(x, "InterpolativePosition::set_point_count");
    if (t <= 0) {
        throw InvArg("InterpolativePosition::set_point_count: point count must "
                     "be positive integer.");
    }
    m_position      = 0.;
    m_current_point = 0;

    if (m_destination > t) { m_destination = 0; }
    m_segment_count = t - 1;
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
    using IntPos = InterpolativePosition;
    ts::TestSuite suite;
    suite.start_series("InterpolativePosition tests");
    suite.test([]() {
        IntPos ip;
        ip.set_behavior(IntPos::k_foreward);
        ip.set_point_count(4);
        assert(ip.point_count() == 4);
        ip.move_position(0.55);
        auto res = ip.move_position(0.55);
        return ts::test(res > 0.05 && ip.current_segment().source == 1);
    });
    suite.test([]() {
        IntPos ip;
        ip.set_behavior(IntPos::k_foreward);
        ip.set_point_count(4);
        for (int i = 0; i != 2*4; ++i) {
            ip.move_position(0.55);
        }
        return ts::test(ip.current_segment().target == ip.current_segment().source);
    });
    suite.test([]() {
        IntPos ip;
        ip.set_behavior(IntPos::k_cycles);
        ip.set_point_count(4);
        for (int i = 0; i != 2*3; ++i) {
            ip.move_position(0.55);
        }
        ip.move_position(0.55);
        return ts::test(   ip.current_segment().target == 0
                        && ip.current_segment().source == 3);
    });
    suite.test([]() {
        IntPos ip;
        ip.set_behavior(IntPos::k_idle);
        ip.set_point_count(4);
        ip.set_whole_position(2.3);
        return ts::test(   ip.current_segment().source == 2
                        && ip.current_segment().target == 3
                        && magnitude(ip.position() - 0.3) < k_error);
    });
    suite.test([]() {
        IntPos ip;
        ip.set_behavior(IntPos::k_idle);
        ip.set_point_count(4);
        ip.set_whole_position(2.3);
        ip.move_position(0.8);
        return ts::test(   ip.current_segment().source == 2
                        && ip.current_segment().target == 3);
    });
    suite.test([]() {
        try {
            IntPos ip;
            ip.set_position(IntPos::k_toward_destination);
        } catch (...) {
            return ts::test(true);
        }
        return ts::test(false);
    });
    suite.test([]() {
        IntPos ip;
        ip.set_point_count(4);
        ip.target_point(2);
        assert(ip.behavior() == IntPos::k_toward_destination);
        for (int i = 0; i != 2*4; ++i) {
            ip.move_position(0.55);
        }
        return ts::test(   ip.current_segment().source == ip.current_segment().target
                        && ip.current_segment().source == 2);
    });
    suite.test([]() {
        IntPos ip;
        ip.set_point_count(4);
        ip.set_whole_position(3.);
        return ts::test(   ip.current_segment().source == ip.current_segment().target
                        && ip.current_segment().source == 3);

    });
    suite.test([]() {
        IntPos ip;
        ip.set_point_count(4);
        ip.target_point(1);
        ip.set_whole_position(3.);
        ip.move_position(0.55);
        auto res = ip.move_position(0.55);
        return ts::test(   ip.current_segment().source == 2
                        && ip.current_segment().target == 1
                        && res > 0.05);
    });
}

/* private */ InterpolativePosition::SegUInt
    InterpolativePosition::previous_point() const noexcept
{
    const auto last_point = m_segment_count;
    switch (m_behavior) {
    case k_idle:
    case k_foreward:
        if (m_current_point == 0) return k_no_point_back;
        return m_current_point - 1;
    case k_cycles:
        if (m_current_point == 0) return last_point;
        return m_current_point - 1;
    case k_toward_destination:
        if (m_current_point == m_destination) {
            // I'm not sure what's the best value for this...
            return m_current_point;
        } else if (m_current_point > m_destination) {
            return (m_current_point == last_point) ? 0 : m_current_point + 1;
        } else {
            return (m_current_point == 0) ? last_point : m_current_point - 1;
        }
    }
    std::cerr << "InterpolativePosition::previous_point: bad branch" << std::endl;
    std::terminate();
}

/* private */ InterpolativePosition::SegUInt
    InterpolativePosition::next_point() const noexcept
{
    const auto last_point = m_segment_count;
    switch (m_behavior) {
    case k_idle    :
    case k_foreward:
        if (m_current_point == last_point) return k_no_point_back;
        return m_current_point + 1;
    case k_cycles  :
        if (m_current_point == last_point) return 0;
        return m_current_point + 1;
    case k_toward_destination:
        if (m_current_point == m_destination) {
            return k_no_point_back;
        } else if (m_current_point > m_destination) {
            return (m_current_point == 0) ? last_point : m_current_point - 1;
        } else {
            return (m_current_point == last_point) ? 0 : m_current_point + 1;
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
