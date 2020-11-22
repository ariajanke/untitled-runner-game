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

#include "SurfaceRef.hpp"
#include "Maps.hpp"

#include "../Components.hpp"

#include <cassert>

namespace {

using Error = std::runtime_error;

} // end of <anonymous> namespace

/* static */ const VectorI SurfaceRef::k_no_location = VectorI(-1, -1);

void SurfaceRef::set(const LineMapLayer & layer, VectorI tile_location_, int segnum) {
    assert(segnum >= 0);
    m_layer           = &layer;
    m_tile_location   = to_small_vec(tile_location_);
    m_segment_number  = segnum;
    m_attached_entity = ecs::EntityRef();
    check_invarients();
}

void SurfaceRef::set(ecs::EntityRef eref, int segnum) {
    assert(segnum >= 0);
    m_layer           = nullptr;
    m_tile_location   = SmallVec(); // to_small_vec(k_no_location);
    m_segment_number  = segnum;
    m_attached_entity = eref;
    check_invarients();
}

void SurfaceRef::move_to_segment(int segnum) {
    if (!m_layer) {
        throw std::runtime_error("SurfaceRef::move_to_segment: reference does not refer to the map.");
    }
    auto count = m_layer->get_segment_count(VectorI(m_tile_location));
    if (segnum < 0 || segnum >= count) {
        throw std::out_of_range("SurfaceRef::move_to_segment: given segment number does not refer to any segment on the map at the current tile location.");
    }
    m_segment_number = segnum;
    check_invarients();
}

VectorI SurfaceRef::tile_location() const
    { return VectorI(m_tile_location); }

int SurfaceRef::segment_number() const
    { return m_segment_number; }

const ecs::EntityRef & SurfaceRef::attached_entity() const
    { return m_attached_entity; }

bool SurfaceRef::is_equal_to(const SurfaceRef & rhs) const {
    return m_layer           == rhs.m_layer          &&
           m_tile_location   == rhs.m_tile_location  &&
           m_segment_number  == rhs.m_segment_number &&
           m_attached_entity == rhs.m_attached_entity;
}

/* private static */ SurfaceRef::SmallVec SurfaceRef::to_small_vec(VectorI r) {
    for (auto i : { r.x, r.y }) {
        if (i < 0 || i > std::numeric_limits<SmallVecEl>::max()) {
            throw std::out_of_range("SurfaceRef::to_small_vec: failed to shorten vector.");
        }
    }
    return SmallVec(r);
}

/* private */ Surface SurfaceRef::get_surface() const {
    static constexpr const char * k_attached_gone_msg =
        "SurfaceRef::get_surface: Attached platform is missing, cannot get surface.";
    if (m_layer) {
        return (*m_layer)(VectorI(m_tile_location), m_segment_number);
    } else {
        if (!m_attached_entity) throw Error(k_attached_gone_msg);
        Entity e(m_attached_entity);

        const auto * plat = e.ptr<Platform>();
        if (!plat) throw Error(k_attached_gone_msg);

        auto surf = plat->get_surface(m_segment_number);

        if (const auto * pcomp = e.ptr<PhysicsComponent>()) {
            auto loc = pcomp->location();
            surf.a += loc;
            surf.b += loc;
        }
        return surf;
    }
}

/* private */ void SurfaceRef::check_invarients() const {
    if (m_layer) {
        //assert(m_tile_location != k_no_location);
        assert(m_layer->has_position(VectorI(m_tile_location)));
    } else if (m_attached_entity) {
        assert(Entity( m_attached_entity ).has<Platform>());
    }
}
