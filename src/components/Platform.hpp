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

class Waypoints;

// my thoughts on class ECS components
// OK: maintain data's validity
// OK: convience functions organized in class
// NOT OK: actual game logic in class
// NOT OK: "update" per some amount of time
// vector is a proper OOP ~~NOT OK: abstract "proper" OOP like classes~~
// I think the focus should be to divorce *game* behavior from data
class Platform {
public:
#   if 0
    friend SurfaceView make_surface_view(const Platform &, const PhysicsComponent *, const Waypoints *);
#   endif
#   if 0
    using WaypointsPtr = std::shared_ptr<std::vector<VectorD>>;
#   endif
    static constexpr const std::size_t k_no_surface  = std::size_t(-1);
#   if 0
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

    std::size_t next_waypoint() const;

    std::size_t previous_waypoint() const;
#   endif
    // ------------------------- concerning surfaces --------------------------
#if 0
    [[deprecated]] SurfaceView surface_view() const
        { return surface_view_relative(); }
#   endif
    SurfaceView surface_view() const;
#   if 0
    SurfaceView surface_view_relative() const;

    SurfaceView surface_view(const PhysicsComponent *) const;
#   endif
    std::size_t next_surface(std::size_t idx) const noexcept;

    std::size_t previous_surface(std::size_t idx) const noexcept;

    std::size_t surface_count() const { return m_surfaces.size(); }
#   if 0
    [[deprecated]] Surface get_surface(std::size_t idx) const;
#   endif
#   if 0
    Surface get_surface(std::size_t idx, const Waypoints *) const;
#   endif
    Surface get_surface(std::size_t) const;

    void set_surfaces(std::vector<Surface> && surfaces)
        { m_surfaces = std::move(surfaces); }
#if 0
    void move_to(VectorD r);

    void move_by(VectorD delta);
#endif
    void set_offset(VectorD r) { m_offset = r; }
private:
    bool surfaces_cycle() const noexcept;

    VectorD average_location() const;
#   if 0
    VectorD offset(const PhysicsComponent *) const;
#   endif
    std::vector<Surface> m_surfaces;
    VectorD m_offset;
};

class Waypoints {
public:
    using WaypointsContainer = std::vector<VectorD>;
    using WaypointsPtr = std::shared_ptr<WaypointsContainer>;
    enum Behavior { k_cycles, k_idle, k_forewards, k_toward_destination };

    static constexpr const std::size_t k_no_waypoint = std::size_t(-1);

    double position = 0.; // [0 1]
    double speed    = 0.; // px/sec

    // ------------------------- concerning waypoints -------------------------

    WaypointsPtr waypoints;
    std::size_t waypoint_number = k_no_waypoint;
#   if 0
    bool cycle_waypoints = true;
#   endif
    Behavior behavior() const;

    void set_behavior(Behavior);

    std::size_t destination_waypoint() const;

    void set_destination_waypoint(std::size_t);

    VectorD waypoint_offset() const;

    bool has_waypoint_offset() const noexcept;

    LineSegment current_waypoint_segment() const;

    std::size_t next_waypoint() const;

    std::size_t previous_waypoint() const;
#if 0
private:
    VectorD offset(const PhysicsComponent *) const;
#endif
private:
    static constexpr auto k_no_waypoints_assigned_msg =
        "no waypoints have been assigned";
    std::size_t increment_index(bool cycle, std::size_t idx) const;
    std::size_t decrement_index(bool cycle, std::size_t idx) const;

    Behavior m_behavior = k_forewards;
    std::size_t m_dest_waypoint = k_no_waypoint;
};
#if 0
SurfaceView make_surface_view(const Platform &);

SurfaceView make_surface_view(const Platform &, const PhysicsComponent *);

SurfaceView make_surface_view(const Platform &, const Waypoints *);

SurfaceView make_surface_view(const Platform &, const PhysicsComponent *, const Waypoints *);
#endif
