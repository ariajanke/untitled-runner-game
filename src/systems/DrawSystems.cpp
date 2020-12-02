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

#include "DrawSystems.hpp"

#include <SFML/Graphics/RenderTarget.hpp>

#include <cassert>

/* private static */ AnimatorSystem::CharAniUpdate
    AnimatorSystem::compute_animation_update
    (const PhysicsComponent & pcomp/*, bool should_flip_frame*/)
{
    using CharAni = CharacterAnimator;
    int sequence_by_state = CharAni::k_invalid_sequence;
    double time_per_frame = CharAni::k_default_spf;
    if (pcomp.state_is_type<FreeBody>()) {
        const auto & freebody = pcomp.state_as<FreeBody>();
        double ang;
        if (magnitude(freebody.velocity) < k_error) {
            ang = 0.;
        } else {
            ang = angle_between(k_gravity, freebody.velocity);
        }
        if (ang > k_pi*0.25 && ang < k_pi*0.75) {
            sequence_by_state = CharAni::k_spin_jump;
        } else if (ang >= k_pi*0.75) {
            sequence_by_state = CharAni::k_jump;
        } else {
            sequence_by_state = CharAni::k_falling;
        }
    } else if (pcomp.state_is_type<LineTracker>()) {
        const auto & tracker = pcomp.state_as<LineTracker>();
        auto seg = *tracker.surface_ref();
        auto true_speed = magnitude(velocity_along(tracker.speed, seg));
        if (true_speed > CharAni::k_high_run_speed_thershold) {
            sequence_by_state = CharAni::k_high_speed_run;
        } else if (true_speed > k_error) {
            sequence_by_state = CharAni::k_low_speed_run;
        } else {
            sequence_by_state = CharAni::k_idle;
        }
        auto fps = (true_speed / 100.)*(1. / CharAni::k_spf_p100ps);
        if (fps != 0.) {
            // I shouldn't need to worry about actual infinities
            // however zero should be covered
            time_per_frame = 1. / fps;
        } else {
            time_per_frame = std::numeric_limits<double>::infinity();
        }
    } else {
        throw ImpossibleBranchException();
    }
    return CharAniUpdate { sequence_by_state, /*should_flip_frame, */time_per_frame };
}

/* private static */ void AnimatorSystem::update_character_animation
    (double elapsed_time, const CharAniUpdate & ani_update, CharacterAnimator & animator)
{
#   if 0
    auto ani_update = compute_animation_update(pstate);
#   endif
    if (animator.current_sequence != ani_update.sequence_number) {
        // we must update frame
        animator.frame_time    = 0.;
        animator.current_frame = 0;
        animator.current_sequence = ani_update.sequence_number;
    } else if (animator.frame_time + elapsed_time > ani_update.time_per_frame) {
        animator.frame_time = std::fmod((animator.frame_time + elapsed_time), ani_update.time_per_frame);
        animator.current_frame = animator.sprite_sheet->next_frame(animator.current_sequence, animator.current_frame);
    } else {
        animator.frame_time += elapsed_time;
    }
}

// ----------------------------------------------------------------------------

/* private */ void DrawSystem::update(const Entity & e, const SingleImage & simg) {
    sf::Sprite spt;
    spt.setTexture(*simg.texture);
    spt.setTextureRect(simg.texture_rectangle);
    auto loc = e.get<PhysicsComponent>().location();
    if (!e.get<PhysicsComponent>().state_is_type<Rect>()) {
        Rect text_bounds = Rect(simg.texture_rectangle);
        loc -= VectorD(text_bounds.width*0.5, text_bounds.height);
    }
    if (const auto * rt_point = e.ptr<ReturnPoint>()) {
        static constexpr const double k_max_fade_time = 3.;
        double t = 1.;
        if (rt_point->recall_max_time > k_max_fade_time) {
            t = std::min(1., rt_point->recall_time / k_max_fade_time);
        } else {
            t = rt_point->recall_time / rt_point->recall_max_time;
        }
        assert(is_real(t) && t >= 0. && t <= 1.);
        spt.setColor(sf::Color(255, 255, 255, int(std::round(t*255.))));
    }
    spt.setPosition(sf::Vector2f(loc));
    m_sprites.push_back(spt);
}

/* private */ void DrawSystem::render_to(sf::RenderTarget & render_target) {
    for (auto circle : m_circles) {
        m_circle_drawer.set_color(circle.color);
        m_circle_drawer.draw_circle(render_target, circle.location);
    }
    for (auto line : m_lines) {
        m_ldrawer.draw_line(render_target, line.a, line.b);
    }
    for (const auto & spt : m_sprites) {
        render_target.draw(spt);
    }
}
