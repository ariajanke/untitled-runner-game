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

#include <memory>

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

// ----------------------------------------------------------------------------

// my thoughts on class ECS components
// OK: maintain data's validity
// OK: convience functions organized in class
// NOT OK: actual game logic in class
// NOT OK: "update" per some amount of time
// vector is a proper OOP ~~NOT OK: abstract "proper" OOP like classes~~
// I think the focus should be to divorce *game* behavior from data
class Platform {
public:
    static constexpr const std::size_t k_no_surface  = std::size_t(-1);

    SurfaceView surface_view() const;

    std::size_t next_surface(std::size_t idx) const noexcept;

    std::size_t previous_surface(std::size_t idx) const noexcept;

    std::size_t surface_count() const { return m_surfaces.size(); }

    Surface get_surface(std::size_t) const;

    void set_surfaces(std::vector<Surface> && surfaces)
        { m_surfaces = std::move(surfaces); }

    void set_offset(VectorD r) { m_offset = r; }

private:
    bool surfaces_cycle() const noexcept;

    VectorD average_location() const;

    std::vector<Surface> m_surfaces;
    VectorD m_offset;
};

// ----------------------------------------------------------------------------

class InterpolativePosition {
public:
    enum Behavior : uint8_t {
        k_foreward, // position advances from one point to the other, maxing
                    // out at the end point
        k_cycles  , // same as forwards, but cycles around end to 0
        k_idle    , // describes non-movement, position fixed at the start
        // movement toward a set destination
        k_toward_destination
    };
    struct SegPair { std::size_t target, source; };

    // has a position, speed, and segment
    static constexpr const auto k_no_point = std::size_t(-1);
    static const SegPair k_no_segment_pair;

    // ---------------------------- whole position ----------------------------

    /** @brief moves the position within the segment. If the position hits 1 or
     *         0, then the segment number is changed, and the remainder is
     *         returned.
     *  @returns positive real number, the remainder after the position hit
     *           the end of the segment. Otherwise 0 is returned.
     */
    double move_position(double);

    /** @param x maybe [0 n + m] where n is number of segments,
     *           and m is in [0 1]
     */
    void set_whole_position(double x);

    // --------------------------- position inside ----------------------------

    /** sets position within segment */
    void set_position(double);

    double position() const noexcept;

    // -------------------------------- speed ---------------------------------

    void set_speed(double);

    double speed() const noexcept;

    // ------------------------------ behavioral ------------------------------

    void set_behavior(Behavior);

    Behavior behavior() const noexcept;

    void target_point(std::size_t);

    std::size_t targeted_point() const noexcept;

    // ---------------------------- which segment -----------------------------

    // second is target, first is source
    SegPair current_segment() const;

    /**
     *  @note may change destination point
     */
    void set_segment_count(std::size_t);

    void set_segment_source(std::size_t);

    std::size_t point_count() const noexcept { return m_segment_count + 1; }

    std::size_t segment_count() const noexcept { return m_segment_count; }

    static void run_tests();

private:
    using SegUInt = uint16_t;
    static constexpr const SegUInt k_no_point_back = SegUInt(-1);

    SegUInt previous_point() const noexcept;
    SegUInt next_point    () const noexcept;

    void check_invarients() const;
    static SegUInt verify_fit(std::size_t, const char * caller);

    Behavior m_behavior     = k_idle;
    SegUInt m_current_point = 0;
    SegUInt m_segment_count = 1; // note: seg count == point count
    SegUInt m_destination   = 0; // irrelevent if m_behavior != k_toward_destination

    double m_speed    = 0.;
    double m_position = 0.;
};

class Waypoints {
public:
    using Container    = std::vector<VectorD>;
    using ContainerPtr = std::shared_ptr<const Container>;

    Waypoints & operator = (ContainerPtr sptr) {
        m_sptr = sptr;
        return *this;
    }

    operator const Container & () const { return points(); }

    const Container & points() const {
        if (!m_sptr) throw std::runtime_error("Waypoints::points: container pointer not set");
        return *m_sptr;
    }

    const Container * operator -> () const { return m_sptr.get(); }
    const Container operator * () const { return *m_sptr; }

    // should be true for all active entities
    bool has_points() const noexcept { return m_sptr != nullptr; }

private:
    ContainerPtr m_sptr = nullptr;
};

// a == source, b == target
inline LineSegment get_waypoint_segment
    (const Waypoints::Container & pts, const InterpolativePosition & intpos)
{
    auto cseg = intpos.current_segment();
    return LineSegment { pts.at(cseg.source), pts.at(cseg.target) };
}

inline LineSegment get_waypoint_segment
    (const Waypoints & waypts, const InterpolativePosition & intpos)
{ return get_waypoint_segment(waypts.points(), intpos); }

inline VectorD get_waypoint_location
    (const Waypoints::Container & pts, const InterpolativePosition & intpos)
{
    if (intpos.point_count() != pts.size()) {
        throw std::invalid_argument("get_waypoint_location: number of waypoints "
              "do not match points on the interpolative position component.");
    }
    auto seg = get_waypoint_segment(pts, intpos);
    auto t = intpos.position();
    return seg.a*t + seg.b*(1. - t);
}

inline VectorD get_waypoint_location
    (const Waypoints & waypts, const InterpolativePosition & intpos)
{ return get_waypoint_location(waypts.points(), intpos); }

#if 0
class Waypoints {
public:
    using WaypointsContainer = std::vector<VectorD>;
    using WaypointsPtr = std::shared_ptr<WaypointsContainer>;
    enum Behavior { k_cycles, k_idle, k_forewards, k_toward_destination };

    static constexpr const std::size_t k_no_waypoint = std::size_t(-1);
#   if 0
    double position = 0.; // [0 1]
    double speed    = 0.; // px/sec
    std::size_t waypoint_number = k_no_waypoint;
#   endif

    void set_speed_pxs(double); // must be real number, pixels per second
    void set_position(double); // [0 1] within segment
    /** Changes waypoints at most once, return
     *
     * @return
     */
    double move_position(double);

    double speed() const; // segments per second


    // ------------------------- concerning waypoints -------------------------
#   if 0
    WaypointsPtr waypoints;
#   endif
    void set_waypoints(WaypointsPtr);

    // ------------------------------- behavior -------------------------------

    Behavior behavior() const;

    void set_behavior(Behavior);

    std::size_t destination_waypoint() const;

    void set_destination_waypoint(std::size_t);

    // ------------------------ local concern waypoints -----------------------

    VectorD waypoint_offset() const;
#   if 0
    [[deprecated]] bool has_waypoint_offset() const noexcept;
#   endif
private:
    LineSegment current_waypoint_segment() const;

    std::size_t next_waypoint() const;

    std::size_t previous_waypoint() const;

private:
    static constexpr auto k_no_waypoints_assigned_msg =
        "no waypoints have been assigned";

    std::size_t increment_index(bool cycle, std::size_t idx) const;
    std::size_t decrement_index(bool cycle, std::size_t idx) const;

    Behavior m_behavior = k_forewards;
    std::size_t m_dest_waypoint = k_no_waypoint;
};
#endif
