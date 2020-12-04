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

#include "SystemsDefs.hpp"

struct EnvColParams;

struct IntersectionInfo final : public SurfaceRef {
    IntersectionInfo() {}

    IntersectionInfo(const SurfaceRef & ref_, VectorD intersection_):
        SurfaceRef(ref_), intersection(intersection_) {}

    VectorD intersection = k_no_intersection;
};

static constexpr const std::size_t k_intersections_in_place_length = 8;

using IntersectionsVec = std::vector<
    IntersectionInfo,
    DefineInPlace<k_intersections_in_place_length>::Allocator<IntersectionInfo>>;

void handle_freebody_physics(EnvColParams &, VectorD new_pos);

void add_map_intersections
    (const EnvColParams &, IntersectionsVec &, VectorD old_pos, VectorD new_pos);

void add_platform_intersections
    (const EnvColParams &, IntersectionsVec &, VectorD old_pos, VectorD new_pos);

/// sorts intersections with the nearest coming first
void sort_intersections(IntersectionsVec &, VectorD old_pos);
