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

#include <SFML/Graphics/Texture.hpp>

#include <common/MultiType.hpp>

#include <memory>

namespace sf { class Sprite; }

// not a component
class SpriteSheet {
public:
    void load_from_file(const char *);
    void load_from_file(const std::string & s) { load_from_file(s.c_str()); }
    void bind_to(sf::Sprite &, int sequence_number, double sequence_time) const;
    void bind_to(sf::Sprite &, int sequence_number, int sequence_frame) const;
    int sequence_count() const noexcept { return m_seq_offsets.empty() ? 0 : int(m_seq_offsets.size() - 1); }
    int next_frame(int sequence_number, int frame_number) const noexcept;
    const sf::IntRect & frame(int sequence_number, int sequence_frame) const;
    std::size_t total_frame_count() const noexcept;

private:
    sf::Texture m_texture;
    std::vector<sf::IntRect> m_frames;
    std::vector<int> m_seq_offsets;
};

// ----------------------------- components here ------------------------------

struct ColorCircle {
    sf::Color color;
    double radius = 5.;
};

struct CharacterAnimator {
    static constexpr const int k_invalid_sequence = -1;

    static constexpr const int k_jump           = 0;
    static constexpr const int k_bonk           = 1;
    static constexpr const int k_spin_jump      = 2;
    static constexpr const int k_falling        = 3;
    static constexpr const int k_low_speed_run  = 4;
    static constexpr const int k_high_speed_run = 5;
    static constexpr const int k_idle           = 6;

    static constexpr const double k_high_run_speed_thershold = 500.;

    // for running
    // seconds per frame, per 100 pixels per second
    // scales linearly(?) from infinity at 0        pixels per seconds
    //                    to   0        at infinity pixels per seconds
    static const constexpr double k_spf_p100ps = 1./ 8.;
    // for animation unrelated to running
    static const constexpr double k_default_spf = 1. / 8.;

    // the rest of the information can be collected from elsewhere
    int current_sequence   = k_idle;
    int current_frame      = 0;
    // some state needed between frames
    double frame_time      = 0.;

    std::shared_ptr<SpriteSheet> sprite_sheet;
};

struct SingleImage {
    const sf::Texture * texture;
    sf::IntRect texture_rectangle;
};

struct HoloCrate {
    sf::Color color;
    sf::IntRect rect;
};

using DisplayFrame = MultiType<ColorCircle, CharacterAnimator, SingleImage, HoloCrate>;
