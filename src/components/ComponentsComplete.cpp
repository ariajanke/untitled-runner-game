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

#include "ComponentsComplete.hpp"
#include <iostream>

#include <cassert>

LineTracker * get_tracker(Entity e) {
    if (!e.has<PhysicsComponent>()) return nullptr;
    return e.get<PhysicsComponent>().state_ptr<LineTracker>();
}

FreeBody * get_freebody(Entity e) {
    if (!e.has<PhysicsComponent>()) return nullptr;
    return e.get<PhysicsComponent>().state_ptr<FreeBody>();
}

Rect * get_rectangle(Entity e) {
    using Rect = sf::Rect<double>;
    if (!e.has<PhysicsComponent>()) return nullptr;
    return e.get<PhysicsComponent>().state_ptr<Rect>();
}

void add_color_circle(Entity e, sf::Color c, double radius) {
    auto & cir = e.add<DisplayFrame>().reset<ColorCircle>();
    cir.color = c;
    cir.radius = radius;
}

// ----------------------------------------------------------------------------

/* vtable anchor */ void Script::process_control_event(const ControlEvent &) {
    throw std::runtime_error("Script::process_control_event: this script type does not process events.");
}

// ----------------------------------------------------------------------------

PlayerScript::PlayerScript(Entity player_e):
    m_player(player_e)
{}

/* private */ void PlayerScript::process_control_event
    (const ControlEvent & control_event)
{
    auto & pcon = m_player.get<PlayerControl>();
    switch (control_event.type_id()) {
    case k_press_event:
        switch (control_event.as<PressEvent>().button) {
        case ControlMove::move_left : press_left (pcon); break;
        case ControlMove::move_right: press_right(pcon); break;
        case ControlMove::jump      : pcon.jump_held = true; break;
        case ControlMove::use:
            pcon.grabbing = true;
            if (m_player.get<Collector>().held_object())
                pcon.will_release = true;
            break;
        default: throw BadBranchException();
        }
        break;
    case k_release_event:
        switch (control_event.as<ReleaseEvent>().button) {
        case ControlMove::move_left : release_left (pcon); break;
        case ControlMove::move_right: release_right(pcon); break;
        case ControlMove::jump      : pcon.jump_held = false; break;
        case ControlMove::use:
            pcon.grabbing = false;
            if (pcon.will_release) {
                pcon.will_release = false;
                pcon.releasing    = true ;
            }
            break;
        default: throw BadBranchException();
        }
        break;
    case ControlEvent::k_no_type: break;
    default: throw BadBranchException();
    }

}

/* private */ void PlayerScript::on_departing(Entity, EntityRef) {}

/* private */ void PlayerScript::on_landing(Entity, VectorD, EntityRef) {}

// ----------------------------------------------------------------------------

namespace {

using LandFunc   = void (ScalePivotScript::*)(VectorD, EntityRef);
using DepartFunc = void (ScalePivotScript::*)(EntityRef);

template <LandFunc land_f, DepartFunc depart_f>
class ScalePartScript final : public Script {
public:
    using SharedPivotScriptPtr = std::shared_ptr<ScalePivotScript>;
    ScalePartScript(SharedPivotScriptPtr pivot): m_pivot(pivot) {}
    void on_landing(Entity, VectorD hit_velocity, EntityRef other) override {
        assert(m_pivot);
        ((*m_pivot).*land_f)(hit_velocity, other);
    }
    void on_departing(Entity, EntityRef other) override {
        assert(m_pivot);
        ((*m_pivot).*depart_f)(other);
    }
private:
    SharedPivotScriptPtr m_pivot = nullptr;
};

} // end of <anonymous> namespace

/* static */ void ScalePivotScript::add_pivot_script_to
    (Entity pivot, Entity left, Entity right)
{
    auto sptr = std::make_shared<ScalePivotScript>(pivot, left, right);
    left.add<ScriptUPtr>() = std::make_unique<ScalePartScript<
        &ScalePivotScript::land_left, &ScalePivotScript::leave_left>
    >(sptr);
    right.add<ScriptUPtr>() = std::make_unique<ScalePartScript<
        &ScalePivotScript::land_right, &ScalePivotScript::leave_right>
    >(sptr);
}

ScalePivotScript::ScalePivotScript(Entity pivot, Entity left, Entity right):
    m_pivot(pivot),
    m_left(left),
    m_right(right)
{
#   if 0
    Rect pivot_bounds = m_pivot.get<PhysicsComponent>().state_as<Rect>();
    for (auto part : { m_left, m_right }) {
        Rect bounds = part.get<PhysicsComponent>().state_as<Rect>();
        part.remove<PhysicsComponent>();
        auto & plat = part.add<Platform>();
        auto & waypts = part.add<Waypoints>();
        plat.set_surfaces(std::vector<Surface>{ Surface(LineSegment(VectorD(), VectorD(bounds.width, 0.))) });
        VectorD top_pt(bounds.left, bounds.top + bounds.height*0.5);

        waypts.waypoints = std::make_shared<std::vector<VectorD>>(std::vector<VectorD> {
            top_pt, VectorD(top_pt.x, pivot_bounds.top + pivot_bounds.height*0.5)
        });
        waypts.set_behavior(Waypoints::k_forewards);
#       if 0
        waypts.cycle_waypoints = false;
#       endif
        waypts.speed = 50.;
        waypts.waypoint_number = 0;
        waypts.position = 0.;
    }
#   endif
}

void ScalePivotScript::land_left(VectorD, EntityRef) {
    ++m_left_weight;
    print_out_weights();
    update_balance();
}

void ScalePivotScript::land_right(VectorD, EntityRef) {
    ++m_right_weight;
    print_out_weights();
    update_balance();
}

void ScalePivotScript::leave_left(EntityRef) {
    --m_left_weight;
    print_out_weights();
    update_balance();
}

void ScalePivotScript::leave_right(EntityRef) {
    --m_right_weight;
    print_out_weights();
    update_balance();
}

/* private */ void ScalePivotScript::print_out_weights() const {
    std::cout << "weights (l:r) (" << m_left_weight << ":" << m_right_weight << ") balance: " << (m_left_weight - m_right_weight) << std::endl;
}

void ScalePivotScript::update_balance() {
#   if 0
    auto & left_waypts = m_left.get<Waypoints>();
    auto & right_waypts = m_right.get<Waypoints>();

    if (m_left_weight > m_right_weight) {
        left_waypts.speed = -50.;
        right_waypts.speed = 50.;
    } else if (m_left_weight < m_right_weight) {
        left_waypts.speed = 50.;
        right_waypts.speed = -50.;
    } else {
        left_waypts.speed = right_waypts.speed = 0.;
    }
#   endif
}

/* private */ void BasketScript::on_departing(Entity e, EntityRef other_ref) {
    if (!is_simple_item(Entity(other_ref))) return;
    if (e.is_requesting_deletion()) return;
    change_weight(-1, e.get<InterpolativePosition>());
}

/* private */ void BasketScript::on_landing(Entity e, VectorD, EntityRef other_ref) {
    if (!is_simple_item(Entity(other_ref))) return;
    if (e.is_requesting_deletion()) return;
    change_weight(1, e.get<InterpolativePosition>());
}

/* private */ void BasketScript::on_update(Entity e, double) {
    if (!e.has<InterpolativePosition>()) return;
    auto & intpos = e.get<InterpolativePosition>();
    if (   intpos.current_segment().target == intpos.current_segment().source
        && intpos.current_segment().target == intpos.point_count() - 1)
    {
        m_basket_wall.request_deletion();
    }
    if (m_seg != intpos.current_segment()) {
        std::cout << "Segment (source: " << intpos.current_segment().source
                  << " target: " << intpos.current_segment().target << ") targeting "
                  << intpos.targeted_point() << std::endl;
        m_seg = intpos.current_segment();
    }
}

/* private static */ bool BasketScript::is_simple_item(const Entity & e) {
    if (const auto * item = e.ptr<Item>())
        { return item->hold_type == Item::simple; }
    return false;
}

/* private */ void BasketScript::change_weight(int delta, InterpolativePosition & intpos) {
    assert(magnitude(delta) == 1);
    m_held_weight += delta;

    std::cout << "Weight " << m_held_weight;
    intpos.target_point(std::min(std::size_t(std::max(m_held_weight, 0)), intpos.point_count()));
    std::cout << " dest waypoint " << intpos.targeted_point() << std::endl;
}

#if 0
/* private */ void PrintOutLandingsDepartingsScript::on_departing
    (Entity this_ent, EntityRef eref)
{
    std::cout << eref.hash() << " has departed from " << this_ent.hash() << std::endl;
}

/* private */ void PrintOutLandingsDepartingsScript::on_landing
    (Entity this_ent, VectorD, EntityRef eref)
{
    std::cout << eref.hash() << " has landed on " << this_ent.hash() << std::endl;
}
#endif
