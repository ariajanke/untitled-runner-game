/****************************************************************************

    Copyright 2021 Aria Janke

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

#include <SFML/Graphics/Drawable.hpp>

#include <common/DrawRectangle.hpp>

class Flower final : public sf::Drawable {
public:
    void setup(std::default_random_engine &);

    void update(double et);

    void set_location(double x, double y) { m_location = VectorD(x, y); }
    void set_location(VectorD r) { m_location = r; }

    VectorD location() const { return m_location; }

    double width() const { return m_petals.width(); }

    double height() const {
        // should be good enough
        return m_petals.height()*0.5 + m_stem.height();
    }

private:
    using UInt8Distri = std::uniform_int_distribution<uint8_t>;
    static constexpr const uint8_t k_max_u8 = std::numeric_limits<uint8_t>::max();

    // Just a really long time, I really want finite values
    static constexpr const double k_initial_time = 3600.;
    void draw(sf::RenderTarget &, sf::RenderStates) const override;

    double petal_thershold() const noexcept;

    double resettle_thershold() const noexcept;

    double pistil_pop_time() const noexcept;

    void pop_pistil(double amount);

    void pop_petal(double amount);

    static void verify_0_1_interval(const char * caller, double amt);

    static sf::Color random_petal_color(std::default_random_engine &);

    static sf::Color random_pistil_color(std::default_random_engine &);

    static sf::Color random_stem_color(std::default_random_engine &);

    DrawRectangle m_petals;
    DrawRectangle m_pistil;
    DrawRectangle m_stem  ;

    // animation works like this:
    // settled -> pistil pop -> petal pop -> settled

    double m_popped_position = 0.;

    double m_time               = 0.;
    double m_to_pistil_pop      = k_initial_time;
    double m_time_at_pistil_pop = k_initial_time;
    double m_to_petal_pop       = k_initial_time;
    double m_time_at_petal_pop  = k_initial_time;

    VectorD m_location;
};

