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

#include <SFML/Graphics/Sprite.hpp>

class AnimatorSystem final : public System, public TimeAware {
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

class DrawSystem final : public System, public GraphicsAware {
    void update(const ContainerView & cont) override
        { for (auto & e : cont) update(e); }

    void update(const Entity &);

    void update(const Entity &, const ColorCircle &);

    void update(const Entity &, const CharacterAnimator &);

    void update(const Entity & e, const SingleImage &);

    static constexpr const std::size_t k_max_loc_history = 3;
    std::unordered_map<Entity, std::vector<VectorD>, EntityHasher> m_previous_positions;
};

class PlatformDrawer final : public System, public GraphicsAware {
    void update(const ContainerView & cont) override {
        for (auto e : cont) {
            if (!e.has<Platform>()) continue;
            update(e.get<Platform>());
        }
    }

    void update(const Platform & platform) {
        for (LineSegment surface : platform.surface_view()) {
            graphics().draw_line(surface.a, surface.b, sf::Color::Cyan, 3.);
        }
    }
};
