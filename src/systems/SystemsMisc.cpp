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

#include "SystemsMisc.hpp"

#include "../maps/Maps.hpp"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>

#include <algorithm>
#include <iostream>

#include <cassert>

// things to implement with basic physics [~6/10]
// slide landing*
// - maintain as much velocity as possible when landing
// intersegment displacement transfer*
// - left over elapsed time is now handled
// - displacement *is* still lost when flying off a segment
// - further testing is needed, rigorious (scripted) tests would be best
// - acceleration is a serious problem on densely segmented tiles
// launch events*
// - for jumping, throwing, etc...
// terminal and minimal velocities
// - universal terminal velocity
// - maximum and minimum upward launch speed (cull/force jumps)
// - maximum speed on a surface
// bounce/slide discrimination*
// - player seldom bounces
// - some objects love to bounce
// ignoring gravity*
// fall off speed
// - need something better than the current implementation
// friction
// - different for different surfaces
// wall hits*
// - when hitting 90 degree change onto a surface that's clearly a wall
// - a bug remains, the player needs to slide down the wall
// acceleration specific to the player

/* private */ void PlayerControlSystem::update(Entity e) {
    if (!e.has<PlayerControl>()) return;
    auto & pcon = e.get<PlayerControl>();

    if (pcon.control_lock != PlayerControl::k_unlocked) return;

    if (auto * fbody = get_freebody(e)) {
        handle_freebody_running(*fbody, pcon);
    } else if (auto * tracker = get_tracker(e)) {
        EntityRef carrying;
        if (const auto * col = e.ptr<Collector>()) {
            carrying = col->held_object();
        }
        handle_tracker_running(*tracker, pcon, carrying);
        handle_tracker_jumping(e.get<PhysicsComponent>(), *tracker, pcon);
    }
    if (direction_of(pcon) < 0.) {
        pcon.last_direction = PlayerControl::k_left;
    } else if (direction_of(pcon) > 0.) {
        pcon.last_direction = PlayerControl::k_right;
    }
}

/* private */ void PlayerControlSystem::handle_freebody_running
    (FreeBody & freebody, const PlayerControl & pcon)
{
    static const auto rot_grav = normalize(rotate_vector(k_gravity, -k_pi*0.5));
    double breaking_boost = 1.;
    if (!are_very_close(freebody.velocity, VectorD()) &&
        pcon.direction != PlayerControl::k_neither_dir)
    {
        if (angle_between(rot_grav*direction_of(pcon), freebody.velocity) > k_error) {
            breaking_boost = k_breaking_boost;
        }
    }

    auto vdel = rot_grav*direction_of(pcon)*k_acceleration*elapsed_time()*breaking_boost;
    bool new_vel_would_excede = magnitude(freebody.velocity + vdel) > k_max_voluntary_speed;
    if (magnitude(freebody.velocity) > k_max_voluntary_speed && new_vel_would_excede) {
        // do nothing
    } else if (new_vel_would_excede) {
        freebody.velocity = normalize(freebody.velocity)*k_max_voluntary_speed;
    } else {
        freebody.velocity += vdel;
    }
}

/* private */ void PlayerControlSystem::handle_tracker_running
    (LineTracker & tracker, const PlayerControl & pcon, EntityRef carrying)
{
    // vdel needs to be converted to "segments per seconds"
    auto seg = *tracker.surface_ref();// get_surface(map_layer, tracker);
    auto pixels_per_length = magnitude(seg.a - seg.b);
    // (pix / s) * (length / pix)
    auto dir = (tracker.inverted_normal ? 1. : -1.)*direction_of(pcon);
    auto vdel = k_acceleration*elapsed_time()*dir / pixels_per_length;
    auto max_vs_segplen = k_max_voluntary_speed / pixels_per_length;

    if (carrying) {
        auto htype = Entity(carrying).get<Item>().hold_type;
        if (htype == Item::run_booster) {
            vdel *= k_booster_factor;
            max_vs_segplen *= 1.5;
        }
    }

    if (tracker.speed*dir < 0) {
        vdel *= k_breaking_boost;
    }
    if (magnitude(tracker.speed) > max_vs_segplen && tracker.speed*dir > 0) {
        // do nothing
    } else if (magnitude(tracker.speed + vdel) > max_vs_segplen) {
        tracker.speed = normalize(tracker.speed)*max_vs_segplen;
    } else {
        tracker.speed += vdel;
    }

}

/* private static */ void PlayerControlSystem::handle_tracker_jumping
    (PhysicsComponent & pcomp, const LineTracker & tracker, PlayerControl & pcon)
{
    if (!pcon.jump_held) return;

    pcon.jump_time = 0.4; // maximum of four seconds
    auto segment = *tracker.surface_ref();
    auto normal  = normal_for(tracker);
    FreeBody freebody;
    freebody.location = location_along(tracker.position, segment) +
                        normal*k_error;
    freebody.velocity = velocity_along(tracker.speed, segment) +
                        normal*k_jump_speed;
    pcomp.reset_state<FreeBody>() = freebody;
}

// ----------------------------------------------------------------------------

/* private */ void ExtremePositionsControlSystem::update(Entity & e) {
    if (!e.has<PhysicsComponent>()) return;
    const auto & map_layer = get_map_layer(e);
    auto right_extreme = double(map_layer.width())*map_layer.tile_width();
    if (auto * fb = get_freebody(e)) {
        if (fb->location.x <= 0.) {
            fb->location.x = k_error;
            fb->velocity.x = 10.;
        } else if (fb->location.x >= right_extreme) {
            fb->location.x = right_extreme - k_error;
            fb->velocity.x = -10.;
        } else if (fb->location.y < -100.) {
            fb->location.y = -100.;
        } else if (fb->location.y > map_layer.height()*map_layer.tile_height()) {
            if (auto * rt = e.ptr<ReturnPoint>()) {
                const auto & pcomp = Entity(rt->ref).get<PhysicsComponent>();
                if (const auto * rect = pcomp.state_ptr<Rect>()) {
                    fb->location = center_of(*rect);
                } else {
                    fb->location = pcomp.location();
                }
                fb->velocity = VectorD();
            } else {
                e.request_deletion();
            }
        }
    }
}

// ----------------------------------------------------------------------------

/* private */ void SnakeSystem::update(const ContainerView & view) {
    for (auto & e : view) {
        if (!e.has<Snake>()) continue;
        update(e);
    }
}

/* private */ void SnakeSystem::update(Entity & e) const {
    auto & snake = e.get<Snake>();
    if (snake.instances_remaining == 0) {
        e.request_deletion();
        return;
    }

    snake.elapsed_time += elapsed_time();
    if (snake.elapsed_time < snake.until_next_spawn) return;

    auto new_ball = e.create_new_entity();
    new_ball.add<Lifetime>();
    new_ball.add<DisplayFrame>().
        reset<ColorCircle>().color = instance_color(snake);
    new_ball.add<PhysicsComponent>().
        reset_state<FreeBody>().location = snake.location;

    snake.elapsed_time = 0.;
    --snake.instances_remaining;
}

/* private static */ sf::Color SnakeSystem::instance_color(const Snake & snake) {
    sf::Color rv;
    int n = snake.total_instances;
    int x = n - snake.instances_remaining;
    auto b = snake.begin_color;
    auto e = snake.end_color;
    rv.r = component_average(n, x, b.r, e.r);
    rv.g = component_average(n, x, b.g, e.g);
    rv.b = component_average(n, x, b.b, e.b);
    return rv;
}

// ----------------------------------------------------------------------------

/* private */ void GravityUpdateSystem::update(Entity e) {
    if (!e.has<PhysicsComponent>()) return;
    if (const auto * pcomp = e.ptr<PhysicsComponent>()) {
        if (!pcomp->affected_by_gravity) return;
    }
    double multiplier = 1.0;
    if (auto * col = e.ptr<Collector>()) {
        if (col->held_object()) {
            // all held objects should also be Items
            if (Entity(col->held_object()).get<Item>().hold_type == Item::heavy)
                multiplier = 1.5;
        }
    }
    if (auto * fb = get_freebody(e)) {
        fb->velocity += k_gravity*elapsed_time()*multiplier;
    }
    if (auto * tracker = get_tracker(e)) {
        auto attached_seg = *tracker->surface_ref();
        auto proj = project_onto(k_gravity*multiplier, attached_seg.a - attached_seg.b);
        auto acc = magnitude(proj)*elapsed_time() / segment_length(attached_seg);
        auto ab_angle = angle_between(attached_seg.a - attached_seg.b, k_gravity);
        auto ba_angle = angle_between(attached_seg.b - attached_seg.a, k_gravity);
        if (ab_angle < ba_angle) {
            acc *= -1;
        }
        if (magnitude(ab_angle - k_pi*0.5) < k_pi*0.1 ||
            magnitude(ba_angle - k_pi*0.5) < k_pi*0.1)
        { acc = 0; }
        tracker->speed += acc;
    }
}
#if 0
// ----------------------------------------------------------------------------

void ItemCollisionSystem::update(const ContainerView & cont) {
    m_collectors.clear();
    m_items.clear();
    for (auto & e : cont) {
        if (e.has<PhysicsComponent>() && e.has<Collector>())
            { m_collectors.push_back(e); }
        if (get_rectangle(e) && e.has<Item>()) { m_items.push_back(e); }
    }
    check_item_collection();
    for (auto e : m_collectors) {
        e.get<Collector>().last_location = e.get<PhysicsComponent>().location();
    }
}

void ItemCollisionSystem::check_item_collection() {
    for (auto & item : m_items) {
    for (auto & collector : m_collectors) {
        check_item_collection(collector, item);
    }}
}

void ItemCollisionSystem::check_item_collection(Entity collector, Entity item) {
    auto & item_rect = *get_rectangle(item);

    const auto & col_comp = collector.get<Collector>();
    auto offset = col_comp.collection_offset;
    if (col_comp.last_location == Collector::k_no_location) return;
    auto cur_location = collector.get<PhysicsComponent>().location() + offset;
    auto old_location = col_comp.last_location + offset;

    if (   !item_rect.contains(old_location)
        && line_crosses_rectangle(item_rect, old_location, cur_location))
    {
        item.request_deletion();
        ++collector.get<Collector>().diamond;
    }
}
#endif
// ----------------------------------------------------------------------------

/* private */ void TriggerBoxSystem::update(const ContainerView & view) {
    // this system handles an intersection of entities having a different
    // set of component types: Subject x Box
    // Subject Entity:
    // (TriggerBoxSubjectHistory, PhysicsComponent, Collector, ReturnPoint)
    // Box Entity:
    // (TriggerBox, PhysicsComponent)
    m_subjects.clear();
    for (auto e : view) {
        if (is_subject(e)) {
            m_subjects.emplace_back(e, e.ensure<TriggerBoxSubjectHistory>());
            continue;
        }

        auto * tbox = e.ptr<TriggerBox>();
        Rect * rect = nullptr;
        if (auto * pcomp = e.ptr<PhysicsComponent>()) {
            rect = pcomp->state_ptr<Rect>();
        }
        if (!tbox || !rect) continue;
        if (tbox->ptr<CheckPoint>()) {
            m_checkpoints.add(e);
        } else if (tbox->ptr<Launcher>()) {
            m_launchers.add(e);
        } else if (tbox->ptr<ItemCollectionSharedPtr>()) {
            m_item_checker.add(e);
        } else if (tbox->ptr<TargetedLauncher>()) {
            m_target_launchers.add(e);
        }
        if (get_script(e)) {
            m_scripts.add(e);
        }
    }

    m_checkpoints     .do_checks(m_subjects);
    m_launchers       .do_checks(m_subjects);
    m_item_checker    .do_checks(m_subjects);
    m_scripts         .do_checks(m_subjects);
    m_target_launchers.do_checks(m_subjects);

    for (auto & [e, history] : m_subjects) {
        // post this, they're equal u.u
        TriggerBoxSubjectHistoryAtt::set_location(history, e.get<PhysicsComponent>().location());
    }
}

/* private */ void TriggerBoxSystem::BaseChecker::do_checks
    (const SubjectContainer & cont)
{
    for (const auto & [e, history] : cont) {
        for (const auto & cp_ent : m_trespassees) {
            auto old_loc = history.last_location();
            auto new_loc = e.get<PhysicsComponent>().location();
            auto bounds  = get_rect(cp_ent);
            VectorD offset;
            adjust_collision(e, offset, bounds);
            new_loc += offset;
            old_loc += offset;
            if (!is_entering_box(old_loc, new_loc, bounds)) continue;
            handle_trespass(cp_ent, e);
        }
    }
    m_trespassees.clear();
}

/* private */ void TriggerBoxSystem::CheckPointChecker::handle_trespass
    (Entity checkpoint, Entity e) const
{
    if (!e.has<PlayerControl>()) return;
    auto * rt_point = e.ptr<ReturnPoint>();
    if (!rt_point) return;
    if (rt_point->ref == checkpoint) return;

    // "activate" checkpoint
    auto rect = checkpoint.get<PhysicsComponent>().state_as<Rect>();
    VectorD top_mid(rect.left, rect.top);
    VectorD top_bottom = top_mid + VectorD(0., rect.height);
    graphics().post_flag_raise(e, top_bottom, top_mid);
    rt_point->ref = checkpoint;
}

/* private */ void TriggerBoxSystem::ItemChecker::handle_trespass
    (Entity collectable, Entity e) const
{
    auto & gfx = graphics();
    [&]() {
        auto ptr = collectable.get<TriggerBox>().as<ItemCollectionSharedPtr>();
        if (!ptr) return;
        const auto & collect_info = *ptr;
        auto * collector = e.ptr<Collector>();
        if (!collector) return;
        collector->diamond += collect_info.diamond_quantity;
        const auto & rect = collectable.get<PhysicsComponent>().state_as<Rect>();
        gfx.post_item_collection(VectorD(rect.left, rect.top), ptr);
    } ();
    collectable.request_deletion();
}

static VectorD expansion_for_collector(const Entity &);

/* private */ void TriggerBoxSystem::ItemChecker::adjust_collision
    (const Entity & collector, VectorD & offset, Rect & rect) const
{
    auto * col_comp = collector.ptr<Collector>();
    if (!col_comp) return;
    offset = col_comp->collection_offset;
    auto examt = expansion_for_collector(collector);
    rect.left   -= examt.x;
    rect.width  += examt.x*2.;
    rect.top    -= examt.y;
    rect.height += examt.y*2.;
}

static VectorD expansion_for_collector(const Entity & e) {
    auto * df    = e.ptr<DisplayFrame    >();
    auto * pcomp = e.ptr<PhysicsComponent>();
    if (!df || !pcomp) return VectorD();
    auto * char_ani = df->as_pointer<CharacterAnimator>();
    if (!char_ani) return VectorD();
    if (!char_ani->sprite_sheet) return VectorD();
    auto frame = char_ani->sprite_sheet->frame(char_ani->current_sequence, char_ani->current_frame);
    return VectorD(frame.width / 4, frame.height / 3);
}

/* protected static */ void TriggerBoxSystem::LaunchCommonBase::do_set
    (VectorD launch_vel, PhysicsComponent & pcomp, PlayerControl * pcont)
{
    if (pcont) {
        pcont->control_lock = PlayerControl::k_until_tracker_locked;
    }

    if (pcomp.state_is_type<LineTracker>()) {
        FreeBody freebody;
        freebody.location = get_detached_position(pcomp.state_as<LineTracker>());
        pcomp.reset_state<FreeBody>() = freebody;
    }
    assert(pcomp.state_is_type<FreeBody>());
    pcomp.state_as<FreeBody>().velocity = launch_vel;
}

/* protected static */ VectorD TriggerBoxSystem::LaunchCommonBase::get_detached_position
    (const LineTracker & tracker)
{
    return location_along(tracker.position, *tracker.surface_ref())
           + normal_for(tracker)*k_error;
}

/* private */ void TriggerBoxSystem::LauncherChecker::handle_trespass
    (Entity launch_e, Entity e) const
{
    auto & launcher = launch_e.get<TriggerBox>().as<Launcher>();
    auto & pcomp    = e.get<PhysicsComponent>();
    auto launch_vel = launcher.launch_velocity.expand();
    switch (launcher.type) {
    case Launcher::k_booster:
        return do_boost(launch_vel, pcomp);
    case Launcher::k_detacher:
        return do_launch(launch_vel, pcomp);
    case Launcher::k_setter:
        return do_set(launch_vel, pcomp, e.ptr<PlayerControl>());
    default: throw std::runtime_error("An unset launcher made it into the game.");
    }
}

/* private static */ void TriggerBoxSystem::LauncherChecker::do_boost
    (VectorD launch_vel, PhysicsComponent & pcomp)
{
    if (auto * tracker = pcomp.state_ptr<LineTracker>()) {
        auto seg = *tracker->surface_ref();
        auto proj = project_onto(launch_vel, seg.b - seg.a);
        auto boost = magnitude(proj); // px/s
        auto current_velocity = tracker->speed*(seg.b - seg.a); // px/s

        // change boost direction depending on segment's direction
        if (magnitude(normalize(proj) - normalize(seg.b - seg.a)) >= k_error) {
            boost *= -1;
        }

        // regular boost
        if (magnitude(current_velocity + proj) < k_max_boost) {
            // convert to segments per second
            tracker->speed += (boost / magnitude(seg.b - seg.a));
        } else if (magnitude(current_velocity) > k_max_boost) {
            // unaffected by boost
        }
        // cull the boost
        else if (magnitude(current_velocity + proj) > k_max_boost) {
            tracker->speed = k_max_boost*normalize(boost) / magnitude(seg.b - seg.a);
        }
    } else if (auto * freebody = pcomp.state_ptr<FreeBody>()) {
        if (magnitude(freebody->velocity + launch_vel) < k_max_boost) {
            freebody->velocity += launch_vel;
        } else if (magnitude(freebody->velocity) > k_max_boost) {
            // no effect
        } else if (magnitude(freebody->velocity + launch_vel) > k_max_boost) {
            freebody->velocity = normalize(freebody->velocity + launch_vel)*k_max_boost;
        }
    }
}

/* private static */ void TriggerBoxSystem::LauncherChecker::do_launch
    (VectorD launch_vel, PhysicsComponent & pcomp)
{
    FreeBody freebody;
    VectorD cur_velocity;
    if (pcomp.state_is_type<FreeBody>()) {
        freebody = pcomp.state_as<FreeBody>();
        cur_velocity = freebody.velocity;
    } else if (pcomp.state_is_type<LineTracker>()) {
        const auto & tracker = pcomp.state_as<LineTracker>();
        freebody.location = get_detached_position(tracker);
        cur_velocity = velocity_along(tracker.speed, *tracker.surface_ref());
    }

    freebody.velocity = launch_vel;
    if (cur_velocity != VectorD()) {
        freebody.velocity += project_onto(cur_velocity, rotate_vector(launch_vel, k_pi*0.5));
    }
    pcomp.reset_state<FreeBody>() = freebody;
}

/* private */ void TriggerBoxSystem::TargetedLauncherChecker::handle_trespass
    (Entity launch_e, Entity e) const
{
    const auto & target_launcher = launch_e.get<TriggerBox>().as<TriggerBox::TargetedLauncher>();
    Entity target_e(target_launcher.target);
    Rect target_bounds = target_e.get<PhysicsComponent>().state_as<Rect>();
    Rect source_bounds = launch_e.get<PhysicsComponent>().state_as<Rect>();

    auto & pcomp = e.get<PhysicsComponent>();
    auto vel = std::get<1>(compute_velocities_to_target(
        center_of(source_bounds), center_of(target_bounds),
        k_gravity, target_launcher.speed));
    if (!is_real(vel)) {
        throw std::runtime_error("Attempting to launch with an insufficient speed, fix was supposed to occur at map loading time.");
    }
    do_set(vel, pcomp, e.ptr<PlayerControl>());
    pcomp.state_as<FreeBody>().location = center_of(source_bounds);
}

/* private */ void TriggerBoxSystem::ScriptChecker::handle_trespass
    (Entity being_hit, Entity whats_hitting) const
{
    get_script(being_hit)->on_box_hit(being_hit, whats_hitting);
}

/* static */ bool TriggerBoxSystem::is_subject(const Entity & e) {
    if (auto * pcomp = e.ptr<PhysicsComponent>()) {
        if (pcomp->state_is_type<FreeBody>() || pcomp->state_is_type<LineTracker>()) {
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------

/* private */ void HoldItemSystem::update(const ContainerView & view) {
    for (auto * c : { &m_holders, &m_holdables }) c->clear();
    for (auto e : view) {
        auto * pcomp = e.ptr<PhysicsComponent>();
        if (!pcomp) continue;
        if (e.has<PlayerControl>() && e.has<Collector>())
            { m_holders.push_back(e); }
        if (e.has<Item>()) {
            if (e.get<Item>().hold_type != Item::not_holdable)
                { m_holdables.push_back(e); }
        }
    }
    for (auto hldr : m_holders) {
        for (auto hldbl : m_holdables)
            { check_to_hold(hldr, hldbl); }
    }
    for (auto held : m_holdables) {
        check_release(held);
    }
    for (auto hldr : m_holders) {
        clear_player_con_releases(hldr);
    }
}

/* private static */ void HoldItemSystem::check_to_hold(Entity holder, Entity holdable) {
    auto & holder_pcomp = holder.get<PhysicsComponent>();

    // can only grab things while on the ground, and grabbing, and not
    // currently holding an entity
    if (!holder_pcomp.state_is_type<LineTracker>() ||
        !holder.get<PlayerControl>().grabbing     ||
         holder.get<Collector>().held_object()      ) return;

    auto & holdable_pcomp = holdable.get<PhysicsComponent>();
    auto holdable_loc     = holdable_pcomp.location();

    auto holder_loc = holder_pcomp.location() + holder.get<Collector>().collection_offset;

    if (magnitude(holdable_loc - holder_loc) >= 30.) return;

    auto & hstate = holdable_pcomp.reset_state<HeldState>();
    pick_up_item(holder, holdable);
    if (holdable.get<Item>().hold_type != Item::jump_booster ||
        holdable_pcomp.bounce_thershold < 1000.                ) return;
    hstate.set_release_func([](EntityRef holder_ref) {
        auto & holder_pcomp = Entity(holder_ref).get<PhysicsComponent>();
        if (auto * fb = holder_pcomp.state_ptr<FreeBody>()) {
            static constexpr const double k_jump_boost = 333.;
            static constexpr const double k_max_boost  = 1.25;
            auto fb_g = project_onto(fb->velocity, k_gravity);
            auto fb_a = fb->velocity - fb_g;

            fb_g -= normalize(k_gravity)*k_jump_boost;
            if (   magnitude(angle_between(fb_g, k_gravity)) < k_error
                || magnitude(fb_g) < k_jump_boost)
            {
                // speed minimum
                fb_g = -normalize(k_gravity)*k_jump_boost;
            } else if (magnitude(fb_g) > k_jump_boost*k_max_boost) {
                // speed maximum
                fb_g = normalize(fb_g)*k_jump_boost*k_max_boost;
            }
            fb->velocity = fb_g + fb_a;
        }
    });

    if (auto * script = get_script(holdable))
        script->on_held(holdable, holder);
}

/* private static */ void HoldItemSystem::check_release(Entity held) {
    auto & held_pcomp  = held.get<PhysicsComponent>();
    if (!held_pcomp.state_is_type<HeldState>()) return;

    Entity holder = [&held, &held_pcomp]() {
        const auto & held_state = held_pcomp.state_as<HeldState>();
        if (held_state.holder()) return Entity(held_state.holder());
        held.request_deletion();
        return Entity();
    } ();

    if (!holder) return;
    if (!holder.get<PlayerControl>().releasing) return;

    release_held_item(holder);

    auto & fb = held.get<PhysicsComponent>().reset_state<FreeBody>();
    fb.location = hand_point_of<HeadOffset, PhysicsComponent>(holder);
    fb.velocity = holder.get<PhysicsComponent>().velocity()
                  + VectorD(direction_of(holder.get<PlayerControl>())*40, 0);

    double dir_adj_comp = 0.;
    const auto * pcon = holder.ptr<PlayerControl>();
    bool is_jump_booster = held.get<Item>().hold_type == Item::jump_booster;
    if (pcon && !is_jump_booster) {
        dir_adj_comp = pcon->last_direction == PlayerControl::k_right ? 1. : -1.;
    }

    auto gunit  = normalize(k_gravity);
    auto throwv = rotate_vector(-gunit, k_pi*0.15)*dir_adj_comp;

    VectorD v = (!is_jump_booster) ? -gunit : gunit;
    static constexpr const auto k_throw_speed = 275.;
    v = normalize(v + throwv)*k_throw_speed;
    fb.velocity = v;

    if (auto * script = get_script(held))
        script->on_release(held, holder);
}
