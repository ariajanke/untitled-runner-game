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

#include "Defs.hpp"
#include "PhysicsComponent.hpp"

#include <memory>

class Platform;

class SurfaceIter {
public:
    using ContainerType = std::vector<Surface>;
    using IterBack = ContainerType::const_iterator;

    explicit SurfaceIter(IterBack itr_, VectorD offset_ = VectorD()):
        m_itr(itr_), m_offset(offset_) {}

    SurfaceIter operator ++ ();

    Surface operator * () const { return move_surface(*m_itr, m_offset); }

    Surface operator -> () const { return move_surface(*m_itr, m_offset); }

    int operator - (const SurfaceIter & rhs) const { return m_itr - rhs.m_itr; }

    SurfaceIter operator + (int i) const { return SurfaceIter(m_itr + i, m_offset); }
    bool operator == (const SurfaceIter & rhs) const { return  is_same(rhs); }
    bool operator != (const SurfaceIter & rhs) const { return !is_same(rhs); }

private:
    bool is_same(const SurfaceIter & rhs) const { return m_itr == rhs.m_itr; }
    IterBack m_itr;
    VectorD m_offset;
};

class SurfaceView {
public:
    using IterBack = SurfaceIter::IterBack;
    SurfaceView(IterBack beg_, IterBack end_, VectorD offset_):
        m_offset(offset_), m_beg(beg_), m_end(end_)
    {}
    SurfaceIter begin() const { return SurfaceIter(m_beg, m_offset); }
    SurfaceIter end  () const { return SurfaceIter(m_end); }
    int size() const { return m_end - m_beg; }
private:
    VectorD m_offset;

    IterBack m_beg, m_end;
};

// my thoughts on class ECS components
// OK: maintain data's validity
// OK: convience functions organized in class
// NOT OK: actual game logic in class
// NOT OK: "update" per some amount of time
// NOT OK: abstract "proper" OOP like classes
class Platform {
public:
    using WaypointsPtr = std::shared_ptr<std::vector<VectorD>>;

    static constexpr const std::size_t k_no_surface  = std::size_t(-1);
    static constexpr const std::size_t k_no_waypoint = std::size_t(-1);

    // let's seperate waypoints
    // because some platforms have none!

    double position = 0.; // [0 1]
    double speed    = 0.; // px/sec

    // ------------------------- concerning waypoints -------------------------

    WaypointsPtr waypoints;
    std::size_t waypoint_number = k_no_waypoint;
    bool cycle_waypoints = true;

    VectorD waypoint_offset() const;

    bool has_waypoint_offset() const noexcept;

    LineSegment current_waypoint_segment() const;

    // ------------------------- concerning surfaces --------------------------

    [[deprecated]] SurfaceView surface_view() const
        { return surface_view_relative(); }

    SurfaceView surface_view_relative() const;

    SurfaceView surface_view(const PhysicsComponent *) const;

    std::size_t next_surface(std::size_t idx) const noexcept;

    std::size_t previous_surface(std::size_t idx) const noexcept;

    std::size_t surface_count() const { return m_surfaces.size(); }

    Surface get_surface(std::size_t idx) const;

    void set_surfaces(std::vector<Surface> && surfaces)
        { m_surfaces = std::move(surfaces); }

    std::size_t next_waypoint() const;

    std::size_t previous_waypoint() const;

    void move_to(VectorD r);

    void move_by(VectorD delta);

private:
    bool surfaces_cycle() const noexcept;

    VectorD average_location() const;

    VectorD offset(const PhysicsComponent *) const;

    std::vector<Surface> m_surfaces;
};
#if 0
class PlatformAttachedEntities {
public:
    void add_reference(EntityRef ref) { m_refs.push_back(ref); }
    std::vector<EntityRef> retrieve_entity_references()
        { return std::move(m_refs); }
private:
    std::vector<EntityRef> m_refs;
};
#endif
