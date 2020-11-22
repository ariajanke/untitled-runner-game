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

#include "../Defs.hpp"

class SurfaceRef {
public:
    static constexpr const int k_no_segment = -1;
    static const VectorI k_no_location;
    void set(const LineMapLayer &, VectorI tile_location_, int segnum);

    void set(ecs::EntityRef, int segnum);

    /** Moves reference to refer to another segment. This requires that the
     *  current tile has a surface available at the given index.
     *  @throws if the tile does not have a segment at the given index
     */
    void move_to_segment(int);

    Surface operator * () const { return get_surface(); }

    bool is_on_map() const { return static_cast<bool>(m_layer); }

    VectorI tile_location() const;

    int segment_number() const;
    const ecs::EntityRef & attached_entity() const;

    bool is_equal_to(const SurfaceRef & rhs) const;
    bool operator == (const SurfaceRef & rhs) const { return  is_equal_to(rhs); }
    bool operator != (const SurfaceRef & rhs) const { return !is_equal_to(rhs); }

    operator bool() const noexcept { return m_segment_number != k_no_segment; }
private:
    using SmallVecEl = uint16_t;
    using SmallVec = sf::Vector2<SmallVecEl>;
    static SmallVec to_small_vec(VectorI);
    Surface get_surface() const;
    void check_invarients() const;
    // surface on line map
    const LineMapLayer * m_layer = nullptr;
    SmallVec m_tile_location = SmallVec(k_no_location);

    // either
    int m_segment_number = k_no_segment;

    // surface on entity
    ecs::EntityRef m_attached_entity;
};
