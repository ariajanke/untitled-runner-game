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

#include "ComponentsMisc.hpp"
#include "ComponentsComplete.hpp"

#include <cassert>

namespace {

void press(PlayerControl &, PlayerControl::Direction);
void release(PlayerControl &, PlayerControl::Direction);

} // end of <anonymous> namespace

/* static */ const VectorD Collector::k_no_location = k_no_intersection;

/* free fn */ double direction_of(const PlayerControl & pc) {
    using Pc = PlayerControl;
    switch (pc.direction) {
    case Pc::k_neither_dir: return 0.;
    case Pc::k_left_only  : case Pc::k_left_last  : return -1.;
    case Pc::k_right_only : case Pc::k_right_last : return  1.;
    }
    throw std::runtime_error("direction_of: direction is invalid value.");
}

/* free fn */ void press_left (PlayerControl & pc)
    { press(pc, PlayerControl::k_left_only ); }

/* free fn */ void press_right(PlayerControl & pc)
    { press(pc, PlayerControl::k_right_only); }

/* free fn */ void release_left (PlayerControl & pc)
    { release(pc, PlayerControl::k_left_only ); }

/* free fn */ void release_right(PlayerControl & pc)
    { release(pc, PlayerControl::k_right_only); }

// ----------------------------------------------------------------------------

/* explicit */ MiniVector::MiniVector(VectorD r) {
    auto m = magnitude(r);
    if (m > k_max) {
        throw std::invalid_argument("MiniVector cannot store a vector of this magnitude; max (" + std::to_string(k_max) + ").");
    }
    m_mag = std::round(m / k_scale);
    auto ang = angle_between(k_unit_start, r);
    if (magnitude(rotate_vector(k_unit_start, ang) - normalize(r)) > k_error) {
        ang = 2.*k_pi - ang;
        assert(magnitude(rotate_vector(k_unit_start, ang) - normalize(r)) < k_error);
    }
    assert(ang >= 0. && ang < 2.*k_pi);
    assert(int(std::round((ang / (2.*k_pi))*k_angle_end)) <= std::numeric_limits<BaseType>::max());
    m_dir = std::round((ang / (2.*k_pi))*k_angle_end);
}

VectorD MiniVector::expand() const noexcept {
    auto ang = k_angle_step*double(m_dir);
    return rotate_vector(k_unit_start, ang)*double(m_mag)*k_scale;
}

/* static */ const VectorD MiniVector::k_unit_start = VectorD(1, 0);
/* static */ const VectorD TriggerBoxSubjectHistory::k_no_location = k_no_intersection;

namespace {

void press(PlayerControl & pc, PlayerControl::Direction dir) {
    using Pc = PlayerControl;
    if (dir != Pc::k_left_only && dir != Pc::k_right_only) {
        throw std::invalid_argument("press: dir may only be either right only or left only");
    }
    auto & pdir = pc.direction;
    switch (pc.direction) {
    case Pc::k_neither_dir: pdir = dir; break;
    case Pc::k_left_only  : if (dir == Pc::k_right_only) pdir = Pc::k_right_last; break;
    case Pc::k_right_only : if (dir == Pc::k_left_only ) pdir = Pc::k_left_last ; break;
    // should not be reachable *usually*, but must be covered!
    case Pc::k_left_last  : if (dir == Pc::k_right_only) pdir = Pc::k_right_last; break;
    case Pc::k_right_last : if (dir == Pc::k_left_only ) pdir = Pc::k_left_last ; break;
    }
}

void release(PlayerControl & pc, PlayerControl::Direction dir) {
    using Pc = PlayerControl;
    if (dir != Pc::k_left_only && dir != Pc::k_right_only) {
        throw std::invalid_argument("press: dir may only be either right only or left only");
    }
    auto & pdir = pc.direction;
    switch (pc.direction) {
    case Pc::k_neither_dir: break;
    case Pc::k_left_only  : if (dir == Pc::k_left_only ) pdir = Pc::k_neither_dir; break;
    case Pc::k_right_only : if (dir == Pc::k_right_only) pdir = Pc::k_neither_dir; break;
    case Pc::k_left_last  : case Pc::k_right_last :
        pdir = (dir == Pc::k_left_only) ? Pc::k_right_only : Pc::k_left_only;
        break;
    }
}

} // end of <anonymous> namespace
