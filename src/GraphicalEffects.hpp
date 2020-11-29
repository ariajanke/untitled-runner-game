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

#include "Defs.hpp"

#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Sprite.hpp>

#include <memory>

class TextDrawer final : public sf::Drawable {
public:
    void load_internal_font();
    void set_text_center  (VectorD, const std::string &);
    void set_text_top_left(VectorD, const std::string &);
    void set_text_center  (VectorD, std::string &&);
    void set_text_top_left(VectorD, std::string &&);
    void move(VectorD);
    std::string take_string();
private:
    static constexpr const int k_font_dim = 8;
    static constexpr const int k_padding  = 1;
    void draw(sf::RenderTarget & target, sf::RenderStates states) const override;

    struct FontInfo {
        sf::Texture char_pool;
        std::unordered_map<char, sf::IntRect> char_rects;
    };

    std::shared_ptr<FontInfo> m_font;
    std::string m_string;
    sf::Sprite m_start_brush; // set at starting position
};
