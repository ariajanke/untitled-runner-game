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

#include "FreeBodyPhysics.hpp"
#include "EnvironmentCollisionSystem.hpp"

#include "../maps/Maps.hpp"

#include <cassert>

// need to code clean

namespace {
#if 0
IntersectionsVec compute_intersections(const EnvColParams &, VectorD);
#endif
void compute_intersections(IntersectionsVec &, const EnvColParams &, VectorD);

bool is_inverted_normal(const LineSegment &, VectorD old_pos, VectorD new_pos);

FreeBody handle_slide
    (const LineSegment & seg, const FreeBody & freebody, bool inverted_normal,
     VectorD & new_pos);

void affect_speed(LineTracker &, const LineSegment &, const FreeBody &);

FreeBody handle_bounce
    (const LineSegment &, const FreeBody &, VectorD new_pos);

// walk through all intersecting segments
// the big thing is to account for layer transitions
class FreeBodyMapIterator {
public:
    FreeBodyMapIterator
        (const LineMap & parent, VectorD src, VectorD dest, Layer * layer);

    SurfaceRef operator * () const;

    void operator ++ ();

    bool is_done() const { return !m_parent; }

private:

    enum Side : uint_fast8_t { k_left, k_right, k_top, k_bottom };

    static LineSegment get_side_of(const LineMap & lmap, Side side, VectorI r);

    static VectorD get_pixel_location(const LineMap & lmap, VectorI r);

    static std::array<Side, 3> get_other_sides(Side side);

    VectorD find_point_in_other_tile
        (VectorI tile_loc, Side other_tiles_side) const;

    void move_to_next_tile();

    void move_to_end() { m_parent = nullptr; }

    void set_view_to(VectorI r);

    static VectorI tile_location_of(const LineMap & lmap, VectorD r);

    VectorI tile_location() const
        { return tile_location_of(*m_parent, m_cur_pos); }

    const LineMap * m_parent = nullptr;
    VectorD m_cur_pos;
    VectorD m_dest_pos;
    SurfaceRef m_current_line;
    Layer * m_current_layer = nullptr;
    bool m_previous_was_transition = false;
};

} // end of <anonymous> namespace

/* free fn */ void handle_freebody_physics(EnvColParams & params, VectorD new_pos) {
    IntersectionsVec intersections;
    intersections.reserve(k_intersections_in_place_length);
    compute_intersections(intersections, params, new_pos);
    auto & freebody = params.state_as<FreeBody>();

    // best answer I can think of...
    // the object isn't really moving with a delta this small
    if (are_very_close(freebody.location, new_pos)) return;

    for (const auto & nfo : intersections) {
        // try to find out the normal
        auto seg = *nfo;
        bool inverted_normal = is_inverted_normal(seg, freebody.location, new_pos);
        auto gang = angle_between(normal_for(seg, inverted_normal), k_gravity);

        if (gang < k_pi*0.65 && gang >= k_pi*0.2) {
            // when the displacement is changed, we may need to check other
            // line segments that may now intersect the new displacement
            // vector
            // unfortunately, this will result in a fairly complex
            // implementation
            // further note on this recursion, how is layer affected?
            // consider: a transition could in this call occur from iterating
            //           the map tiles, but a clipped vector may have no
            //           transition
            // I believe presently, the current implementation is correct
            auto fb = handle_slide(seg, freebody, inverted_normal, new_pos);
            assert(magnitude(fb.location - freebody.location) < k_error);
            params.set_freebody(fb);
            return handle_freebody_physics(params, new_pos);
        } else if (gang < k_pi*0.2) {
            // make all ceilings, soft ceilings
            if (seg.hard_ceilling) {
                auto fb = handle_slide(seg, freebody, inverted_normal, new_pos);
                assert(magnitude(fb.location - freebody.location) < k_error);
                params.set_freebody(fb);
                return handle_freebody_physics(params, new_pos);
            } else {
                continue;
            }
        }
        // bouncy specific behavior
        if (magnitude(freebody.velocity) < params.bounce_thershold) {
            // landing occurs
            LineTracker tracker;

            //tracker.surface_ref = nfo;
            double position = magnitude(nfo.intersection - seg.a) / magnitude(seg.b - seg.a);
            assert(magnitude(location_along(position, seg) - nfo.intersection) < k_error);
            tracker.position = position;
            tracker.inverted_normal = inverted_normal;
            affect_speed(tracker, seg, freebody);
#           if 0
            params.events.record_landing(nfo.attached_entity(), freebody.velocity);
#           endif
            return (void)params.set_landing(tracker, nfo, freebody.velocity);
            //return (void)(params.state = PhysicsState(tracker));
        } else {
            return (void)(params.set_freebody(handle_bounce(seg, freebody, new_pos)));
        }
    }

    auto new_fb = freebody;
    new_fb.location = new_pos;
    return (void)(params.set_freebody(new_fb));
}

void add_map_intersections
    (const EnvColParams & params, IntersectionsVec & intersections,
     VectorD old_pos, VectorD new_pos)
{
    FreeBodyMapIterator itr(params.map, old_pos, new_pos, &params.layer);
    for (; !itr.is_done(); ++itr) {
        const auto nfo = *itr;
        auto intersection = find_intersection(*nfo, old_pos, new_pos);
        if (intersection == k_no_intersection) continue;

        intersections.emplace_back(nfo, intersection);
    }
}

/* free fn */ void add_platform_intersections
    (const EnvColParams & params, IntersectionsVec & intersections,
     VectorD old_pos, VectorD new_pos)
{
    for (Entity platform : params.platforms) {
        auto surface_view = platform.get<Platform>().surface_view(platform.ptr<PhysicsComponent>());
        for (auto itr = surface_view.begin(); itr != surface_view.end(); ++itr) {
            Layer layer = Layer::neither;
            if (auto * pcomp = platform.ptr<PhysicsComponent>())
                { layer = pcomp->active_layer; }

            // skip if platform is on another layer
            if (layer != Layer::neither && layer != params.layer) continue;

            auto intersection = find_intersection(*itr, old_pos, new_pos);
            if (intersection == k_no_intersection) continue;
            SurfaceRef sr;
            sr.set(platform, itr - surface_view.begin());
            intersections.emplace_back(sr, intersection);
        }
    }
}

/* free fn */ void sort_intersections
    (IntersectionsVec & intersections, VectorD old_pos)
{
    std::sort(intersections.begin(), intersections.end(),
        [&old_pos](const IntersectionInfo & lhs, const IntersectionInfo & rhs)
    {
        return magnitude(lhs.intersection - old_pos) <
               magnitude(rhs.intersection - old_pos);
    });
}

namespace {

std::pair<double, VectorD>
    handle_intersection(const LineSegment & seg, VectorD old_, VectorD new_);

VectorD reflect_approach(const LineSegment & seg, VectorD approach);

void compute_intersections(IntersectionsVec & intersections, const EnvColParams & params, VectorD new_pos) {
    auto & freebody = params.state_as<FreeBody>();
#   if 0
    //IntersectionsVec intersections;// auto intersections = make_in_place_vector<IntersectionInfo, k_get_intersections_in_place_length>();
    intersections.reserve(k_intersections_in_place_length);
#   endif
    add_map_intersections(params, intersections, freebody.location, new_pos);
    add_platform_intersections(params, intersections, freebody.location, new_pos);
    sort_intersections(intersections, freebody.location);
#   if 0
    return intersections;
#   endif
}

bool is_inverted_normal(const LineSegment & seg, VectorD old_pos, VectorD new_pos) {
    double a = angle_between(seg.a - seg.b, old_pos - new_pos);
    return angle_between(rotate_vector(seg.a - seg.b, a), old_pos - new_pos) < k_error;
}

// clips vector
FreeBody handle_slide
    (const LineSegment & seg, const FreeBody & freebody, bool inverted_normal,
     VectorD & new_pos)
{
    auto old_pos = freebody.location;
    auto diff = new_pos - old_pos;
    auto seg_norm = normal_for(seg, inverted_normal);
    auto n_comp = project_onto(diff, seg_norm);
    auto p_comp = diff - n_comp;
    assert(magnitude(diff - (n_comp + p_comp)) < k_error);
    auto cull_new_pos = [old_pos, n_comp, p_comp](double x)
        { return old_pos + p_comp + n_comp*x; };
    assert(find_intersection(seg, old_pos, cull_new_pos(0)) == k_no_intersection);
    auto t = find_highest_false([cull_new_pos, &seg, old_pos](double x) {
        return k_no_intersection !=
               find_intersection(seg, old_pos, cull_new_pos(x));
    });
    // using k_error just prolongs it :c
    if (t != 0.) {
        new_pos = cull_new_pos(t);
    } else if (find_intersection(seg, old_pos, old_pos + p_comp) == k_no_intersection) {
        new_pos = old_pos + p_comp;
    } else {
        new_pos = old_pos;
    }

    assert(k_no_intersection == find_intersection(seg, old_pos, new_pos));
    FreeBody rv = freebody;
    if (!are_very_close(freebody.velocity, VectorD())) {
        rv.velocity = freebody.velocity - project_onto(freebody.velocity, seg_norm);
    }
    return rv;
}

void affect_speed(LineTracker & tracker, const LineSegment & seg, const FreeBody & freebody) {
    auto segdiff = seg.b - seg.a;
    tracker.speed = magnitude(project_onto(freebody.velocity, segdiff)) / magnitude(segdiff);
    if (magnitude(tracker.speed) < k_error) return;
    auto tracker_vel = velocity_along(tracker.speed, seg);
    auto proj = project_onto(freebody.velocity, segdiff);
    if (angle_between(tracker_vel, proj) > k_pi*.5) {
        tracker.speed *= -1.;
    }
}

FreeBody handle_bounce
    (const LineSegment & seg, const FreeBody & freebody, VectorD new_pos)
{
    auto old_pos = freebody.location;
    auto gv = handle_intersection(seg, freebody.location, new_pos);
    auto diff = new_pos - freebody.location;
    new_pos = gv.second;

    FreeBody fb = freebody;
    fb.location = new_pos;
    auto reflected = reflect_approach(seg, diff);
    auto culled_potential = magnitude(project_onto((1. - gv.first)*diff, k_gravity));
    fb.velocity = normalize(reflected)*(magnitude(fb.velocity) + culled_potential);
    assert(k_no_intersection == find_intersection(seg, old_pos, new_pos));
    return fb;
}

// --------------------- <anonymous> namespace continued ----------------------

VectorD normal_from_approach(const LineSegment &, VectorD approach);

FreeBodyMapIterator::FreeBodyMapIterator
    (const LineMap & parent, VectorD src, VectorD dest, Layer * layer):
    m_parent(&parent),
    m_cur_pos(src),
    m_dest_pos(dest),
    m_current_layer(layer),
    m_previous_was_transition(false)
{
    if (!layer) {
        throw std::invalid_argument("FreeBodyMapIterator(): layer must not be a nullptr.");
    }
    auto start_loc = tile_location_of(parent, src);
    m_previous_was_transition = parent.point_in_transition(start_loc);
    set_view_to(start_loc);
}

SurfaceRef FreeBodyMapIterator::operator * () const {
    SurfaceRef ref;
    ref.set(m_parent->get_layer(*m_current_layer), m_current_line.tile_location(), m_current_line.segment_number());
    return ref;
}

void FreeBodyMapIterator::operator ++ () {
    auto seg_count = m_parent->get_segment_count(*m_current_layer, m_current_line.tile_location());
    assert(m_current_line.segment_number() != seg_count);
    auto next_segnum = m_current_line.segment_number() + 1;
    if (next_segnum == seg_count) {
        assert(!m_current_line.attached_entity());
        move_to_next_tile();
    } else {
        m_current_line.move_to_segment(next_segnum);
    }
}

/* private static */ LineSegment FreeBodyMapIterator::get_side_of
    (const LineMap & lmap, Side side, VectorI r)
{
    // r is top left to begin with
    VectorI u;
    switch (side) {
    // tl and bl
    case k_left: u = r + VectorI(0, 1); break;
    // tr and br
    case k_right:
        u = r + VectorI(1, 0);
        r += VectorI(1, 1);
        break;
    // tl and tr
    case k_top: u = r + VectorI(1, 0); break;
    // bl and br
    case k_bottom:
        u = r + VectorI(0, 1);
        r += VectorI(1, 1);
        break;
    }
    return LineSegment(get_pixel_location(lmap, r), get_pixel_location(lmap, u));
}

/* private static */ VectorD FreeBodyMapIterator::get_pixel_location
    (const LineMap & lmap, VectorI r)
{
    return VectorD(double(r.x)*lmap.tile_width (),
                   double(r.y)*lmap.tile_height());
}

/* private static */ std::array<FreeBodyMapIterator::Side, 3>
    FreeBodyMapIterator::get_other_sides(Side side)
{
    switch (side) {
    case k_left  : return {         k_right, k_top, k_bottom };
    case k_right : return { k_left,          k_top, k_bottom };
    case k_top   : return { k_left, k_right,        k_bottom };
    case k_bottom: return { k_left, k_right, k_top           };
    }
    throw ImpossibleBranchException();
}

/* private */ VectorD FreeBodyMapIterator::find_point_in_other_tile
    (VectorI tile_loc, Side other_tiles_side) const
{
    using namespace std::placeholders;
    auto get_side = std::bind(get_side_of, std::cref(*m_parent), _1, tile_loc);

    auto intr = find_intersection(get_side(other_tiles_side), m_cur_pos, m_dest_pos);
    if (intr == k_no_intersection) return k_no_intersection;

    VectorD other_intr = k_no_intersection;
    for (auto other_side : get_other_sides(other_tiles_side)) {
        auto other_ls = get_side(other_side);
        other_intr = find_intersection(other_ls, m_cur_pos, m_dest_pos);
        if (other_intr != k_no_intersection) break;
    }
    if (other_intr == k_no_intersection) return m_dest_pos;

    auto rv = (other_intr + intr)*0.5;
    assert(tile_location_of(*m_parent, rv) == tile_loc);
    return rv;
}

/* private */ void FreeBodyMapIterator::move_to_next_tile() {
    if (m_cur_pos == m_dest_pos ||
        tile_location() == tile_location_of(*m_parent, m_dest_pos))
    { return move_to_end(); }

    auto cur_loc = tile_location();

    VectorI dx(m_dest_pos.x > m_cur_pos.x ? 1 : -1, 0);
    // side of the next tile
    auto hside = dx.x < 0 ? k_right  : k_left;
    auto gv = find_point_in_other_tile(cur_loc + dx, hside);
    if (gv != k_no_intersection) {
        m_cur_pos = gv;
        return set_view_to(cur_loc + dx);
    }

    VectorI dy(0, m_dest_pos.y > m_cur_pos.y ? 1 : -1);
    auto vside = dy.y < 0 ? k_bottom : k_top ;
    gv = find_point_in_other_tile(cur_loc + dy, vside);
    // else? extend and try again?
    if (gv != k_no_intersection) {
        m_cur_pos = gv;
        return set_view_to(cur_loc + dy);
    }
    throw ImpossibleBranchException();
}

/* private */ void FreeBodyMapIterator::set_view_to(VectorI r) {
    // here lies the magic to the whole enterprise
    // here we switch to the other layer for freebodies
    if (m_parent->point_in_transition(r) && !m_previous_was_transition) {
        *m_current_layer = switch_layer(*m_current_layer);
    }
    m_previous_was_transition = m_parent->point_in_transition(r);
    if (m_parent->get_segment_count(*m_current_layer, r) == 0)
        { return move_to_next_tile(); }

    m_current_line.set(m_parent->get_layer(*m_current_layer), r, 0);
}

/* private static */ VectorI FreeBodyMapIterator::tile_location_of
    (const LineMap & lmap, VectorD r)
{
    return VectorI(int(std::floor(r.x / lmap.tile_width ())),
                   int(std::floor(r.y / lmap.tile_height())));
}

std::pair<double, VectorD>
    handle_intersection(const LineSegment & seg, VectorD old_, VectorD new_)
{
    auto diff = new_ - old_;
    auto cull_new_pos = [old_, diff](double x) { return old_ + diff*x; };

    auto t = find_highest_false([old_, &seg, &cull_new_pos](double x) {
        return k_no_intersection !=
               find_intersection(seg, old_, cull_new_pos(x));
    });
    if (t == 0.) {
        // floating point math is weird sometimes
        // it's possible that old_pos and new_pos has an intersection
        // and for t to be zero
        // this happens when the old position is extremely close to
        // intersecting as is
        new_ = old_;
    } else {
        new_ = cull_new_pos(t);
    }
    return std::make_pair(t, new_);
}

VectorD reflect_approach(const LineSegment & seg, VectorD approach) {
    auto antiapproach = -approach;
    auto normal = normal_from_approach(seg, approach);
    double angle = ::angle_between(normal, antiapproach);
    if (::angle_between(normal, rotate_vector(antiapproach, -angle)) <
        ::angle_between(normal, rotate_vector(antiapproach,  angle)) )
    { angle *= -1.; }
    return rotate_vector(antiapproach, angle*2.);
}

// --------------------- <anonymous> namespace continued ----------------------

VectorD normal_from_approach
    (const LineSegment & seg, VectorD approach)
{
    auto normal = rotate_vector(seg.a - seg.b, k_pi*0.5);
    auto a = angle_between(-normal, -approach);
    auto b = angle_between( normal, -approach);
    return (a < b ? -1. : 1.)*normal;
}

} // end of <anonymous> namespace
