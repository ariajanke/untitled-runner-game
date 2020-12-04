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
#include "../Drawers.hpp"

#include <SFML/Graphics/Sprite.hpp>

class AnimatorSystem final : public System, public TimeAware {
public:
    void update(const ContainerView & cont) override
        { for (auto & e : cont) { update(e); } }

    void update(Entity & e) {
        if (!e.has<DisplayFrame>() || !e.has<PhysicsComponent>()) return;
        if (!e.get<DisplayFrame>().is_type<CharacterAnimator>()) return;
        update_character_animation
            (elapsed_time(),
             compute_animation_update(e.get<PhysicsComponent>()),
             e.get<DisplayFrame>().as<CharacterAnimator>());
    }

    struct CharAniUpdate {
        int sequence_number;
        double time_per_frame;
    };

    static CharAniUpdate compute_animation_update(const PhysicsComponent &);

    static void update_character_animation
        (double elapsed_time, const CharAniUpdate &, CharacterAnimator &);
};

class DrawSystem final : public System, public RenderTargetAware {
public:
    void setup() override {
        m_circle_drawer.set_color(sf::Color::White);
        m_circle_drawer.set_radius(10.);

        m_ldrawer.color = sf::Color::White;
        m_ldrawer.thickness = 3.;
    }
private:
    struct CircleWithLocation : ColorCircle {
        VectorD location;
    };

    void update(const ContainerView & cont) override {
        m_circles.clear();
        m_lines.clear();
        m_sprites.clear();
        for (auto & e : cont) update(e);
    }
    void update(const Entity & e) {
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
    void update(const Entity & e, const ColorCircle & color_circle) {
        CircleWithLocation cwl;
        cwl.color  = color_circle.color;
        cwl.radius = color_circle.radius;
        if (auto holder = get_holder(e.get<PhysicsComponent>())) {
            cwl.location = hand_point_of<HeadOffset, PhysicsComponent>(Entity(holder));
        } else {
            cwl.location = e.get<PhysicsComponent>().location() - VectorD(0, color_circle.radius);
        }
        m_circles.push_back(cwl);
    }
    void update(const Entity & e, const CharacterAnimator & animator) {
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
                m_sprites.push_back(spt);
                if (color.a > 50) color.a -= 50;
                spt.setColor(color);
            }
        } (e, spt);
    }
    void update(const Entity & e, const SingleImage & simg);
    void render_to(sf::RenderTarget & render_target) override;
    CircleDrawer m_circle_drawer;
    LineDrawer m_ldrawer;
    std::vector<CircleWithLocation> m_circles;
    std::vector<LineSegment> m_lines;
    std::vector<sf::Sprite> m_sprites;

    static constexpr const std::size_t k_max_loc_history = 3;
    std::unordered_map<Entity, std::vector<VectorD>, EntityHasher> m_previous_positions;
};

class PlatformDrawer final : public System, public RenderTargetAware {
    void setup() override {
        m_ldrawer.color = sf::Color::Cyan;
        m_ldrawer.thickness = 3.;
    }

    void update(const ContainerView & cont) override {
        m_lines.clear();
        for (auto e : cont) {
            if (!e.has<Platform>()) continue;
            update(e.get<Platform>());
        }
    }

    void update(const Platform & platform) {
        for (LineSegment surface : platform.surface_view()) {
            m_lines.push_back(surface);
        }
    }

    void render_to(sf::RenderTarget & target) override {
        for (const auto & line : m_lines) {
            m_ldrawer.draw_line(target, line.a, line.b);
        }
    }

    std::vector<LineSegment> m_lines;
    LineDrawer m_ldrawer;
};
