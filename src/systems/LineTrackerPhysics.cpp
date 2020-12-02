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

#include "LineTrackerPhysics.hpp"
#include "EnvironmentCollisionSystem.hpp"
#include "FreeBodyPhysics.hpp"

#include "../maps/Maps.hpp"

#include <iostream>

#include <cassert>

namespace {

using PhysicsStateMask = EnvColStateMask;

struct NeighborPosition : public SurfaceRef {
    decltype (LineSegment::k_a) segment_end = LineSegment::k_neither;
};

struct SegTransfer {
    SegTransfer() {}
    SegTransfer(const SurfaceRef & ref_, double angl):
        surface_ref(ref_), angle_of_transfer(angl) {}

    SurfaceRef surface_ref;
    double angle_of_transfer = 0.;
};

struct LinkSegTransfer final : public SegTransfer {
    LinkSegTransfer() {}
    LinkSegTransfer(const NeighborPosition & np_, double ang_):
        SegTransfer(np_, ang_), segment_end(np_.segment_end)
    {}
    LineSegmentEnd segment_end;
};

struct PlatformTransfer final : public SegTransfer {
    static constexpr const double k_never_transfers = std::numeric_limits<double>::infinity();
    PlatformTransfer() {}
    PlatformTransfer(SurfaceRef ref_, double ang_, double et_to_xfer_):
        SegTransfer(ref_, ang_), et_to_transfer(et_to_xfer_)
    {}
    // the very moment *before* a transfer occurs
    double et_to_transfer    = k_never_transfers;
    // the very moment *after* a transfer occurs
    double et_after_transfer = k_never_transfers;
    bool must_invert_normal  = false;
    bool must_flip_speed     = false;
};

using PlatformsCont = EnvColParams::PlatformsCont;

static constexpr const double k_friction_thershold = k_pi*0.8;
#if 0
static constexpr const double k_inf = std::numeric_limits<double>::infinity();
#endif
bool in_segment_range(double x) { return x >= 0. && x <= 1.; }

double check_for_traversal_interruption(EnvColParams &, double et_trav);

void apply_friction(double & tracker_speed, const LineSegment &, double et);

LinkSegTransfer find_linked_transfer
    (const LineTracker &, const LineMapLayer &, LineSegmentEnd);

LineSegmentEnd segment_end_of(double pos);

bool check_for_segment_transfer_interrupt
    (PhysicsStateMask &, const SegTransfer &, double new_pos);

LineTracker transfer_tracker
    (const LinkSegTransfer &, const LineTracker & old_tracker,
     LineSegmentEnd old_seg_end);

void update_layer
    (const LineTracker & old_tracker, const LineTracker & new_tracker,
     const LineMap &, Layer *);

} // end of <anonymous> namespace

/* free fn */ void handle_tracker_physhics(EnvColParams & params, double et) {
    if (et < k_error) return;

    double et_after = 0.;
    double et_trav  = et;
    {
    const auto & tracker = params.state_as<LineTracker>();
    if (!in_segment_range(tracker.position + et*tracker.speed)) {
        auto [port_before, port_after] = find_smallest_diff([&tracker, et](double x) {
            auto new_pos = tracker.position + et*x*tracker.speed;
            return !in_segment_range(new_pos);
        });
        et_trav  = port_before*et;
        et_after = port_after *et;
    }
    }
    // traversal stuff here
    // including being interrupted by a platform segment
    double et_after_itrp = check_for_traversal_interruption(params, et_trav);

    // friction, saps speed, does not affect position
    double new_pos = k_inf;
    if (auto * tracker = params.state_as_ptr<LineTracker>()) {
        // get the new position before modifying speed
        new_pos = tracker->position + tracker->speed*et;
        auto g_n_ang = angle_between(k_gravity, normal_for(*tracker));
        if (!params.acting_will && g_n_ang > k_friction_thershold) {
            apply_friction(tracker->speed, *tracker->surface_ref(), std::min(et_after_itrp, et_trav));
        }
    }

    // zero to interruption taken as "canceling" out further changes
    // (also point of incuring et debt)
    if (et_after_itrp < k_error) return;

    // if traversal was interupted
    // basically restart and try again
    if (et_after_itrp < et_trav) {
        auto rem_et = et_after + (et_trav - et_after_itrp);
        switch (params.state_type_id()) {
        case k_tracker_state:
            return handle_tracker_physhics(params, rem_et);
        case k_freebody_state: {
            const auto & fb = params.state_as<FreeBody>();
            return handle_freebody_physics(params, fb.location + fb.velocity*rem_et);
            }
        default: throw ImpossibleBranchException();
        }
    }
    assert(params.state_is_type<LineTracker>());
    assert(et_after_itrp == k_inf);
    assert(new_pos    != k_inf);
    if (magnitude(params.state_as<LineTracker>().speed) < k_error) return;
    if (et_after <= 0.) return;
    // if we got down here we know two things:
    // - the entity hasn't been interupted
    // - the entity is running of it's current segment
    //
    // it only makes sense to check for layer transition with intersegment
    // transfers... and platform transfers
    LinkSegTransfer segxfer = find_linked_transfer
        (params.state_as<LineTracker>(), params.map.get_layer(params.layer), segment_end_of(new_pos));

    if (check_for_segment_transfer_interrupt(params, segxfer, new_pos)) {
        // incurs et debt here
        return;
    }

    // intersegment transfer
    // we need to preserve the displacement
    // preserve entire displacement and speed, regardless of angle change
    // transfer_tracker
    const auto & old_tracker = params.state_as<LineTracker>();
    LineTracker new_tracker = transfer_tracker(segxfer, old_tracker, segment_end_of(new_pos));

    // this is where we test for layer transitions
    // recall that transition tiles must have the same surfaces for both layers
    update_layer(old_tracker, new_tracker, params.map, &params.layer);
    params.set_transfer(new_tracker);
    handle_tracker_physhics(params, et_after);
} // end of handle_tracker_physhics function

namespace {

PlatformTransfer check_for_platform_transfer
    (const EnvColParams &, const LineTracker &, double fullet);

LineTracker transfer_tracker(const PlatformTransfer &, const LineTracker &);

template <typename T>
sf::Vector2<T> next_after(const sf::Vector2<T> & r, const sf::Vector2<T> & u)
    { return sf::Vector2<T>(std::nextafter(r.x, u.x), std::nextafter(r.y, u.y)); }

double angle_between(const LineSegment & old, const LineSegment & new_,
                     bool inverted_normal_on_old);

LinkSegTransfer find_smallest_angle_neighbor
    (const LineMapLayer &,
     const LineTracker & tracker, LineSegmentEnd);

FreeBody fly_off_tracker_to_freebody(const LineTracker &, double new_pos);

double convert_tracker_speed(const LineTracker & from, const LineSegment & to);

double check_for_traversal_interruption(EnvColParams & params, double et_trav) {
    const auto & tracker = params.state_as<LineTracker>();
    const auto current_seg = *tracker.surface_ref();
    double new_pos = tracker.position + et_trav*tracker.speed;
    assert(in_segment_range(new_pos));

    auto platxfer = check_for_platform_transfer(params, tracker, et_trav);
    if (platxfer.et_to_transfer != PlatformTransfer::k_never_transfers) {
        // have to find a way to unify surface/angle handling rules
        if (platxfer.angle_of_transfer <= k_pi*0.5) {
            params.state_as<LineTracker>().speed = 0.;
            return 0.;
        }

        // update_layer()
        // going to need to update layer for moving platforms
        LineTracker new_tracker = transfer_tracker(platxfer, tracker);
        update_layer(tracker, new_tracker, params.map, &params.layer);
        params.set_transfer(new_tracker);
#       if 0
        params.state = PhysicsState(new_tracker);
#       endif
        return platxfer.et_after_transfer;
    }

    auto norm = normal_for(tracker);
    // falling off from waning momentum on ceiling
    if ( ::angle_between(k_gravity, norm) < k_pi*0.25
        && magnitude(tracker.speed)*segment_length(current_seg) < 50.)
    {
        // have to find a non-intersecting position...
        auto loc = location_along(tracker.position, current_seg);
        FreeBody fbody;
        fbody.location = next_after(loc, loc + norm);
        // want to reduce error, we'll have to handle the remaining et
        // that is another run with the other physics systems with the
        // remaining et...
        // incurs et debt here
        params.set_freebody(fbody);
#       if 0
        params.state = PhysicsState(fbody);
#       endif
        return 0.;
    }
    // that "standard" position update
    LineTracker new_tracker = tracker;
    new_tracker.position = new_pos;
    update_layer(tracker, new_tracker, params.map, &params.layer);
    params.state_as<LineTracker>() = new_tracker;
    assert(in_segment_range(params.state_as<LineTracker>().position));

    return k_inf;
}

void apply_friction(double & tracker_speed, const LineSegment & seg, double et) {
    const constexpr double k_speed_loss_ps  = 0.20;
    const constexpr double k_stop_thershold = 35.;
    if (magnitude(tracker_speed)*segment_length(seg) < k_stop_thershold) {
        tracker_speed = 0.;
    } else {
        tracker_speed *= (1. - k_speed_loss_ps*et);
    }
}

LinkSegTransfer find_linked_transfer(const LineTracker & tracker, const LineMapLayer & map_layer, LineSegmentEnd segend) {
    const auto & surface_ref = tracker.surface_ref();
    // handling for platforms
    if (surface_ref.attached_entity()) {
        const auto & platform = Entity(surface_ref.attached_entity()).get<Platform>();
        auto next_segment = surface_ref.segment_number();
        switch (segend) {
        case LineSegmentEnd::k_a: next_segment = platform.previous_surface(next_segment); break;
        case LineSegmentEnd::k_b: next_segment = platform.next_surface    (next_segment); break;
        default: throw ImpossibleBranchException();
        }
        LinkSegTransfer segxfer;
        if (std::size_t( next_segment ) != Platform::k_no_surface) {
            segxfer.surface_ref.set(surface_ref.attached_entity(), next_segment);
            segxfer.angle_of_transfer = angle_between(*surface_ref, platform.get_surface(next_segment), tracker.inverted_normal);
        }
        return segxfer;
    } else {
        return find_smallest_angle_neighbor(map_layer, tracker, segend);
    }
}

LineSegmentEnd segment_end_of(double pos) {
    if (in_segment_range(pos)) {
        throw std::invalid_argument("segment_end_of: pos must be outside of range [0 1]");
    }
    return (pos < LineSegment::k_a_side_pos) ? LineSegment::k_a : LineSegment::k_b;
}

/// this function may incur et debt
bool check_for_segment_transfer_interrupt
    (PhysicsStateMask & mask, const SegTransfer & segxfer, double new_pos)
{
    // no neighbor fly off, or angle too high
    // maybe have a balence between speed and angle for fly off?
    assert(mask.state_is_type<LineTracker>());
    assert(!in_segment_range(new_pos));
    auto & tracker = mask.state_as<LineTracker>();
    if (!segxfer.surface_ref || segxfer.angle_of_transfer > k_pi*1.25) {
        // this is the only place where a fly off occurs
        // note need to preserve displacement here, but not that high of a priority
        mask.set_freebody(fly_off_tracker_to_freebody(tracker, new_pos));
        return true;
    }
    if (segxfer.angle_of_transfer <= k_pi*0.5) {
        // no transfer (hit wall)
#       if 0
        LineTracker new_tracker = tracker;
#       endif
        tracker.position = (new_pos > 1.) ? 1. - k_error : k_error;
        tracker.speed    = 0.;
        assert(in_segment_range(tracker.position));
#       if 0
        state = PhysicsState(new_tracker);
#       endif
        return true;
    }
    return false;
}

LineTracker transfer_tracker
    (const LinkSegTransfer & link_segxfer, const LineTracker & old_tracker,
     LineSegmentEnd old_seg_end)
{
    LineTracker new_tracker = old_tracker;
    new_tracker.position = (link_segxfer.segment_end == LineSegment::k_a) ? 0. : 1.;
    if (link_segxfer.segment_end == old_seg_end) {
        new_tracker.speed *= -1;
        new_tracker.inverted_normal = !old_tracker.inverted_normal;
    }

    new_tracker.set_surface_ref(link_segxfer.surface_ref);
    new_tracker.speed = normalize(new_tracker.speed)*
        convert_tracker_speed(old_tracker, *new_tracker.surface_ref());
    return new_tracker;
}

void update_layer
    (const LineTracker & old_tracker, const LineTracker & new_tracker,
     const LineMap & lmap, Layer * layer)
{
    const auto & old_segment = *old_tracker.surface_ref();
    const auto & new_segment = *new_tracker.surface_ref();
    auto old_loc = location_along(old_tracker.position, old_segment);
    auto new_loc = location_along(new_tracker.position, new_segment);
    if (!lmap.point_in_transition(old_loc) && lmap.point_in_transition(new_loc))
        { *layer = switch_layer(*layer); }
}

// ----------------------------------------------------------------------------

inline double cross_magnitude(VectorD a, VectorD b) { return a.x*b.y - a.y*b.x; }

template <typename Func>
void find_position_neighbor
    (const LineMapLayer &, const SurfaceRef &, VectorD point_location, Func && f);

PlatformTransfer check_for_platform_transfer
    (const EnvColParams & params, const LineTracker & tracker, double fullet)
{
    //auto intersections = make_in_place_vector<IntersectionInfo, k_get_intersections_in_place_length>();
    DefineInPlaceVector<IntersectionInfo, k_intersections_in_place_length> intersections;
    intersections.reserve(k_intersections_in_place_length);
    auto old_pos = location_of(tracker);
    assert(in_segment_range(tracker.position));
    // I accept that this may or may no be on the actual segment
    auto new_pos = location_along(tracker.position + tracker.speed*fullet, *tracker.surface_ref());
    add_platform_intersections(params, intersections, old_pos, new_pos);

    if (tracker.surface_ref().attached_entity()) {
        add_map_intersections(params, intersections, old_pos, new_pos);
    }

    if (intersections.empty()) return PlatformTransfer();

    sort_intersections(intersections, old_pos);
    const auto & inx = intersections.front();

    {
    static int i = 0;
    std::cout << ++i << " platform transfer ";
    if (inx.attached_entity()) {
        std::cout << inx.attached_entity().hash() << " " << inx.segment_number();
    } else {
        std::cout << "(" << inx.tile_location().x << ", " << inx.tile_location().y << ") "
                  << inx.segment_number();
    }
    std::cout << std::endl;
    }

    PlatformTransfer rv;
    std::tie(rv.et_to_transfer, rv.et_after_transfer) = find_smallest_diff(
        [&tracker, &inx, fullet](double x)
    {
        auto old_loc2d = location_along(tracker.position, *tracker.surface_ref());
        auto new_loc2d = location_along(tracker.position + tracker.speed*fullet*x, *tracker.surface_ref());
        return find_intersection(*inx, old_loc2d, new_loc2d) != k_no_intersection;
    });
    // note tightly coupled with the functor call above
    rv.et_to_transfer    *= fullet;
    rv.et_after_transfer *= fullet;
    assert(find_intersection(
        *inx, location_along(tracker.position, *tracker.surface_ref()),
        location_along(tracker.position + tracker.speed*rv.et_to_transfer, *tracker.surface_ref()))
        == k_no_intersection);
    rv.surface_ref = static_cast<const SurfaceRef &>(inx);
    auto inx_pos = tracker.position + tracker.speed*rv.et_after_transfer;
    [inx, &tracker, inx_pos](PlatformTransfer & rv) {
        LineSegment tracker_seg = *tracker.surface_ref();
        if ( tracker.position < inx_pos )
             { tracker_seg.b = inx.intersection; }
        else { tracker_seg.a = inx.intersection; }
        LineSegment inxa = *inx;
        LineSegment inxb = *inx;
        inxa.a = inx.intersection;
        inxb.b = inx.intersection;
        auto inxa_ang = angle_between(tracker_seg, inxa, tracker.inverted_normal);
        auto inxb_ang = angle_between(tracker_seg, inxb, tracker.inverted_normal);
        const auto * ontoseg = (inxa_ang < inxb_ang) ? &inxa : &inxb;
        bool must_invert = are_very_close(ontoseg->a, tracker_seg.a) ||
                           are_very_close(ontoseg->b, tracker_seg.b);
        rv.angle_of_transfer = std::min(inxa_ang, inxb_ang);
#       if 1
        if (inxa_ang < inxb_ang) {
            std::cout << "b ";
        } else {
            std::cout << "a ";
        }
        std::cout << convert_tracker_speed(tracker, *inx);
        std::cout << std::endl;
#       endif
        rv.must_flip_speed    = !(inxa_ang < inxb_ang);
        rv.must_invert_normal = must_invert;
    }(rv);
    return rv;
}

LineTracker transfer_tracker
    (const PlatformTransfer & platxfer, const LineTracker & old_tracker)
{
    LineTracker new_tracker;
    assert(new_tracker.surface_ref() != platxfer.surface_ref);
    new_tracker.set_surface_ref(platxfer.surface_ref);
    new_tracker.inverted_normal = old_tracker.inverted_normal;
    if (platxfer.must_invert_normal)
        { new_tracker.inverted_normal = !new_tracker.inverted_normal; }

    auto new_speed = convert_tracker_speed(old_tracker, *new_tracker.surface_ref());

    if (platxfer.must_flip_speed   ) new_speed *= -1.;
    new_tracker.speed = new_speed;
    new_tracker.position = [&new_tracker, &old_tracker, new_speed]() {
        // find that first non-intersecting sub segment
        // and get the value as the position for the new tracker
        const LineSegment & new_surface = *new_tracker.surface_ref();
        LineSegment old_segment = *old_tracker.surface_ref();
        // function selection matter low val = pt a, high val pt b
        if (new_speed < 0.) {
            // if we're heading toward a, we want to 'move' pt b
            return find_highest_false([&](double x) {
                return find_intersection(old_segment, location_along(x, new_surface), new_surface.a) !=
                       k_no_intersection;
            });
        } else if (new_speed > 0.) {
            return find_lowest_true([&](double x) {
                return find_intersection(old_segment, location_along(x, new_surface), new_surface.b) ==
                       k_no_intersection;
            });
        }
        throw ImpossibleBranchException();
    } ();
    LineSegment seg, oseg;
    if (new_speed > 0.) {
        seg.b = (*platxfer.surface_ref).b;
        oseg.b = seg.a = location_along(new_tracker.position, *platxfer.surface_ref);
        oseg.a = (*platxfer.surface_ref).a;
    } else {
        seg.a = (*platxfer.surface_ref).a;
        oseg.a = seg.b = location_along(new_tracker.position, *platxfer.surface_ref);
        oseg.b = (*platxfer.surface_ref).b;
    }
    assert(find_intersection(*old_tracker.surface_ref(), seg.a, seg.b) == k_no_intersection);
    assert(in_segment_range(new_tracker.position));
    return new_tracker;
}

double angle_between(const LineSegment & old, const LineSegment & new_,
                     bool inverted_normal_on_old)
{
    const VectorD * pivot     = nullptr;
    const VectorD * extremity = nullptr;
    const VectorD * other_ext = nullptr;
    if (are_very_close(old.a, new_.b)) {
        pivot = &old.a;
        extremity = &old.b;

        other_ext = &new_.a;
    } else if (are_very_close(old.b, new_.a)) {
        pivot = &old.b;
        extremity = &old.a;

        other_ext = &new_.b;
    } else if (are_very_close(old.a, new_.a)) {
        pivot = &old.a;
        extremity = &old.b;

        other_ext = &new_.b;
    } else if (are_very_close(old.b, new_.b)) {
        pivot = &old.b;
        extremity = &old.a;

        other_ext = &new_.a;
    } else {
        throw std::invalid_argument("angle_between: segments do not connect");
    }
    assert(pivot && extremity && other_ext);
    auto saught = normal_for(old, inverted_normal_on_old) + (old.a + old.b)*0.5;
    auto shortest_ang = ::angle_between(*extremity - *pivot, *other_ext - *pivot);

    if (magnitude(shortest_ang - k_pi) < k_error) return k_pi;

    auto cross_z = cross_magnitude(saught - *pivot, *extremity - *pivot);
    assert(cross_z != 0.);
    shortest_ang *= (cross_z > 0.) ? -1. : 1.;
    if (magnitude(normalize(rotate_vector(*extremity - *pivot, shortest_ang)) -
                  normalize(              *other_ext - *pivot              )) < k_error)
    {
        return magnitude(shortest_ang);
    }
    return 2*k_pi - magnitude(shortest_ang);
}

LinkSegTransfer find_smallest_angle_neighbor
    (const LineMapLayer & map_layer, const LineTracker & tracker, LineSegmentEnd end)
{
    NeighborPosition gnp;
    double min_ang = std::numeric_limits<double>::infinity();
    const auto & current_seg = *tracker.surface_ref();
    auto end_point = (end == LineSegment::k_a) ? current_seg.a : current_seg.b;

    // no layer transitions occur here, this is not done during segment
    // transfers
    find_position_neighbor(
        map_layer, tracker.surface_ref(), end_point,
        [&min_ang, &gnp, &current_seg, &tracker]
        (const NeighborPosition & np)
    {
        auto ang = angle_between(current_seg, *np, tracker.inverted_normal);
        if (ang < min_ang) {
            min_ang = ang;
            gnp     = np ;
        }
    });
    return LinkSegTransfer(gnp, min_ang);
}

FreeBody fly_off_tracker_to_freebody
    (const LineTracker & tracker, double new_pos)
{
    FreeBody freebody;
    const auto & current_seg = *tracker.surface_ref();
    auto true_speed = magnitude(tracker.speed)*magnitude(current_seg.a - current_seg.b);
    assert(new_pos < 0. || new_pos > 1.);
    if (new_pos < 0.) {
        auto nadir = current_seg.a + (current_seg.a - current_seg.b);
        freebody.location = next_after(current_seg.a, nadir);
        freebody.velocity = ::normalize(current_seg.a - current_seg.b)*true_speed;
    } else if (new_pos > 1.) {
        auto nadir = current_seg.b + (current_seg.b - current_seg.a);
        freebody.location = next_after(current_seg.b, nadir);
        freebody.velocity = ::normalize(current_seg.b - current_seg.a)*true_speed;
    }
    return freebody;
}

double convert_tracker_speed(const LineTracker & from, const LineSegment & to) {
    auto old_seg = *from.surface_ref();
    auto true_speed = magnitude(from.speed)*segment_length(old_seg);
    return (true_speed / segment_length(to));
}

// ----------------------------------------------------------------------------

template <typename Func>
void find_position_neighbor
    (const LineMapLayer & lmap, const SurfaceRef & lref,
     VectorD point_location, Func && f)
{
    static const auto k_neighbor_offsets = {
        VectorI( 0, 0), // check this tile too
        VectorI(-1, 0), VectorI( 1, 0), VectorI(0, 1), VectorI(0, -1),
        VectorI(-1,-1), VectorI(-1, 1), VectorI(1,-1), VectorI(1,  1)
    };
    if (lref.tile_location() == SurfaceRef::k_no_location) {
        throw std::invalid_argument("find_position_neighbor: surface reference "
                                    "does not refer to the map (platform "
                                    "entities require special handling)");
    }
    for (auto noffset : k_neighbor_offsets) {
        auto tile_pos = noffset + lref.tile_location();
        for (int i = 0; i != lmap.get_segment_count(tile_pos); ++i) {
            // skip originating segment
            if (i == lref.segment_number() && noffset == VectorI(0, 0)) continue;

            LineSegment offseted_ls = lmap(tile_pos, i);
            auto closest_neighbor = LineSegment::k_neither;
            if (are_very_close(offseted_ls.a, point_location)) {
                closest_neighbor = LineSegment::k_a;
            } else if (are_very_close(offseted_ls.b, point_location)) {
                closest_neighbor = LineSegment::k_b;
            } else {
                continue;
            }
            NeighborPosition np;
            np.set(lmap, tile_pos, i);
            np.segment_end = closest_neighbor;
            f(std::cref(np));
        }
    }
}

} // end of <anonymous> namespace
