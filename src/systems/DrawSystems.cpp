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
    (const PhysicsComponent & pcomp)
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
        throw BadBranchException();
    }
    return CharAniUpdate { sequence_by_state, time_per_frame };
}

/* private static */ void AnimatorSystem::update_character_animation
    (double elapsed_time, const CharAniUpdate & ani_update, CharacterAnimator & animator)
{
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

/* private */ void DrawSystem::update(const Entity & e) {
    if (!e.has<DisplayFrame>()) return;
    if (!e.get<PhysicsComponent>().state_is_valid()) return;
    const auto & df = e.get<DisplayFrame>();
    if (df.is_type<ColorCircle>()) {
        update(e, df.as<ColorCircle>());
    } else if (df.is_type<CharacterAnimator>()) {
        update(e, df.as<CharacterAnimator>());
    } else if (df.is_type<SingleImage>()) {
        update(e, df.as<SingleImage>());
    }
}

/* private */ void DrawSystem::update(const Entity & e, const ColorCircle & color_circle) {
    VectorD loc;
    if (auto holder = get_holder(e.get<PhysicsComponent>())) {
        loc = hand_point_of<HeadOffset, PhysicsComponent>(Entity(holder));
    } else {
        loc = e.get<PhysicsComponent>().location() - VectorD(0, color_circle.radius);
    }
    graphics().draw_circle(loc, color_circle.radius, color_circle.color);
}

/* private */ void DrawSystem::update(const Entity & e, const CharacterAnimator & animator) {
    sf::Sprite spt;
    // need to translate for frame's size
    animator.sprite_sheet->bind_to(spt, animator.current_sequence, animator.current_frame);
    VectorD foot_anchor(double(spt.getTextureRect().width)*0.5, double(spt.getTextureRect().height));

    [this](Entity e, VectorD r) {
        auto & vec = m_previous_positions[e];
        vec.push_back(r);
        std::size_t max_history = 1;
        static constexpr const auto k_high_run_speed = CharacterAnimator::k_high_run_speed_thershold;
        if (magnitude(e.get<PhysicsComponent>().velocity()) > k_high_run_speed) {
            max_history = 3;
        }
        if (vec.size() > max_history)
            vec.erase(vec.begin(), vec.begin() + (vec.size() - max_history));
    }(e, e.get<PhysicsComponent>().location());
    spt.setPosition(sf::Vector2f(e.get<PhysicsComponent>().location()));
    spt.setOrigin(sf::Vector2f(foot_anchor));
    if (e.get<PlayerControl>().last_direction == PlayerControl::k_left) {
        spt.setScale(-1.f, 1.f);
    }

    auto pcomp = e.get<PhysicsComponent>();
    if (pcomp.state_is_type<LineTracker>()) {
        static const VectorD k_head_vector(0, -1);
        auto tnorm = normal_for(pcomp.state_as<LineTracker>());
        auto ang = angle_between(tnorm, k_head_vector);
        if (magnitude(rotate_vector(tnorm, ang) - k_head_vector) > k_error) {
            ang *= -1.;
        }
        if (animator.current_sequence != CharacterAnimator::k_idle)
            spt.rotate(-float(ang*180 / k_pi));
    }

    [this](Entity e, sf::Sprite spt) {
        auto color = spt.getColor();
        for (auto r : make_reverse_view(m_previous_positions[e])) {
            spt.setPosition(sf::Vector2f(r));
            graphics().draw_sprite(spt);
            if (color.a > 50) color.a -= 50;
            spt.setColor(color);
        }
    } (e, spt);
}

/* private */ void DrawSystem::update(const Entity & e, const SingleImage & simg) {
    sf::Sprite spt;
    spt.setTexture(*simg.texture);
    spt.setTextureRect(simg.texture_rectangle);
    auto loc = e.get<PhysicsComponent>().location();
    if (!e.get<PhysicsComponent>().state_is_type<Rect>()) {
        Rect text_bounds = Rect(simg.texture_rectangle);
        loc -= VectorD(text_bounds.width*0.5, text_bounds.height);
    }
#   if 0
    else {
        auto rect = e.get<PhysicsComponent>().state_as<Rect>();
        graphics().draw_rectangle(VectorD(rect.left, rect.top), rect.width, rect.height, sf::Color(200, 50, 50, 128));
    }
#   endif
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
    graphics().draw_sprite(spt);
}
