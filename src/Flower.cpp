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

#include "Flower.hpp"

#include <SFML/Graphics/RenderTarget.hpp>

void Flower::setup(std::default_random_engine & rng) {
    using RealDistri = std::uniform_real_distribution<double>;
    auto mk_random_time = [&rng](double max) {
        return RealDistri(0., max)(rng);
    };

    static constexpr const double k_max_pistil_to_pop_time = 0.33;
    static constexpr const double k_max_at_pistil_pop_time = 8.;
    static constexpr const double k_max_petal_to_pop_time  = 0.33;
    static constexpr const double k_max_at_petal_pop_time  = 8.;

    m_to_pistil_pop      = mk_random_time(k_max_pistil_to_pop_time);
    m_time_at_pistil_pop = mk_random_time(k_max_at_pistil_pop_time);
    m_to_petal_pop       = mk_random_time(k_max_petal_to_pop_time );
    m_time_at_petal_pop  = mk_random_time(k_max_at_petal_pop_time );

    static constexpr const double k_min_stem_width = 2.;
    static constexpr const double k_max_stem_width = 4.;

    static constexpr const double k_min_stem_height =  8.;
    static constexpr const double k_max_stem_height = 20.;

    // have origin be the
    double w = RealDistri(k_min_stem_width, k_max_stem_width)(rng);
    m_stem = DrawRectangle(0.f, float(w / 2.), float(w),
         float(RealDistri(k_min_stem_height, k_max_stem_height)(rng)),
         random_stem_color(rng));

    m_pistil = DrawRectangle(0.f, 0.f, w, w, random_pistil_color(rng));

    static constexpr const double k_min_petal_delta = 2.;
    static constexpr const double k_max_petal_delta = 6.;
    {
    double petal_w = RealDistri(k_min_petal_delta, k_max_petal_delta)(rng) + w;
    double petal_h = RealDistri(k_min_petal_delta, k_max_petal_delta)(rng) + w;
    if (petal_w < petal_h)
        std::swap(petal_w, petal_h);
    m_petals = DrawRectangle(0.f, 0.f, float(petal_w), float(petal_h), random_petal_color(rng));
    }

    static constexpr const double k_min_pop_delta = 3.;
    static constexpr const double k_max_pop_delta = 6.;
    m_popped_position = RealDistri(k_min_pop_delta, k_max_pop_delta)(rng);
    pop_pistil(0.);
    pop_petal (0.);

    m_stem.set_x(-m_stem.width()*0.5f);
    m_pistil.set_x(-m_pistil.width()*0.5f);

}

void Flower::update(double et) {
    m_time += et;
    if (m_time > resettle_thershold()) {
        // begin resettling
        // if we resettle all the way, restart (m_time = 0.)
        auto amt = std::min(1., 3.*(m_time - resettle_thershold()) / resettle_thershold());
        pop_pistil(1. - amt);
        pop_petal (1. - amt);
        if (are_very_close(1., amt)) {
            m_time = 0.;
        }
    } else if (m_time > petal_thershold()) {
        // begin popping petal
        pop_pistil(1.);
        pop_petal (std::min(1., (m_time - petal_thershold()) / petal_thershold()));
    } else if (m_time > m_to_pistil_pop) {
        // begin popping pistil
        pop_pistil(std::min(1., (m_time - m_to_pistil_pop) / m_to_pistil_pop));
    }
}

/* private */ void Flower::draw(sf::RenderTarget & target, sf::RenderStates states) const {
    states.transform.translate(sf::Vector2f(m_location));
    target.draw(m_stem  , states);
    target.draw(m_petals, states);
    target.draw(m_pistil, states);
}


/* private */ double Flower::petal_thershold() const noexcept {
    return m_to_pistil_pop + m_time_at_pistil_pop;
}

/* private */ double Flower::resettle_thershold() const noexcept {
    return petal_thershold() + m_to_petal_pop + m_time_at_petal_pop;
}

/* private */ double Flower::pistil_pop_time() const noexcept {
    if (m_to_pistil_pop == k_inf) return 0.;
    return m_time - m_to_pistil_pop;
}

/* private */ void Flower::pop_pistil(double amount) {
    verify_0_1_interval("pop_pistil", amount);

    m_pistil.set_y(-m_petals.height()*0.5f - float(amount*m_popped_position));
}

/* private */ void Flower::pop_petal(double amount) {
    verify_0_1_interval("pop_petal", amount);
    m_petals.set_position(-m_petals.width()*0.5f,
                          -m_petals.height()*0.5f
                          - float(amount*m_popped_position));
}

/* private static */ void Flower::verify_0_1_interval(const char * caller, double amt) {
    if (amt >= 0. || amt <= 1.) return;
    throw std::invalid_argument(std::string(caller) + ": amount must be in [0 1].");
}

/* private static */ sf::Color Flower::random_petal_color
    (std::default_random_engine & rng)
{
    // avoid greens
    auto high  = UInt8Distri(200, k_max_u8 )(rng);
    auto low   = UInt8Distri( 20, high - 40)(rng);
    auto green = UInt8Distri(  0, high - 40)(rng);

    auto red  = high;
    auto blue = low;
    if (UInt8Distri(0, 1)(rng))
        std::swap(red, blue);
    return sf::Color(red, green, blue);
}

/* private static */ sf::Color Flower::random_pistil_color
    (std::default_random_engine & rng)
{
    auto yellow = UInt8Distri(160, k_max_u8)(rng);
    auto blue   = UInt8Distri(0, yellow / 3)(rng);
    return sf::Color(yellow, yellow, blue);
}

/* private static */ sf::Color Flower::random_stem_color
    (std::default_random_engine & rng)
{
    auto green = UInt8Distri(100, k_max_u8)(rng);
    auto others = UInt8Distri(0, green / 3)(rng);
    return sf::Color(others, green, others);
}
