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

#include "GraphicalEffects.hpp"
#include "get_8x8_char.hpp"

#include <common/Grid.hpp>

#include <SFML/Graphics/RenderTarget.hpp>

#include <cstring>
#include <cassert>

namespace {

struct StringView {
    StringView(const char * s_): s(s_) {}

    const char * begin() const noexcept { return s; }
    const char * end  () const noexcept { return s + ::strlen(s); }

    const char * s;
};

enum { k_inner_pix, k_border_pix, k_blank_pix };

inline decltype (k_inner_pix) classify_char(char id, sf::Vector2i r) {
    using Vec = sf::Vector2i;
    static constexpr const int k_size = 8;

    auto is_inside = [](Vec r)
        { return r.x >= 0 && r.y >= 0 && r.x < k_size && r.y < k_size; };
    auto to_index = [](Vec r) { return std::size_t(r.x + r.y*k_size); };
    assert(is_inside(r));

    auto img_str = get_8x8_char(id);
    if (is_on_pixel(img_str[to_index(r)])) return k_inner_pix;
    for (auto n : { Vec(-1, 0), Vec(1, 0), Vec(0, -1), Vec(0,  1) }) {
        n += r;
        if (!is_inside(n)) continue;
        if (is_on_pixel(img_str[to_index(n)])) return k_border_pix;
    }
    return k_blank_pix;
}

} // end of <anonymous> namespace

void TextDrawer::load_internal_font() {
    if (m_font) return;
    m_font = std::make_shared<FontInfo>();
    static constexpr const char * const k_printables =
        "`1234567890-=~!@#$%^&*()_+[]\\{}|;':\",./<>?qwertyuiopasdfghjklzxcvbnm"
        "QWERTYUIOPASDFGHJKLZXCVBNM";

    int len = ::strlen(k_printables);
    m_font->char_rects.reserve(std::size_t(len));

    Grid<sf::Color> pixel_buffer;
    pixel_buffer.set_size(k_font_dim, k_font_dim);
    // assert "horizontality"
    // this is needed for directly uploading the grid data into the texture

    sf::Image preren_img;

    int text_len = int(std::ceil(std::sqrt(double(len))))*(k_font_dim + k_padding);
    preren_img.create(unsigned(text_len), unsigned(text_len), sf::Color(0, 0, 0, 0));
    assert(&*(pixel_buffer.begin() + 1) == &pixel_buffer(1, 0));
    sf::Vector2i write_pos;
    for (auto c : StringView(k_printables)) {
        for (int y = 0; y != k_font_dim; ++y) {
        for (int x = 0; x != k_font_dim; ++x) {
            auto colors = { sf::Color::White, sf::Color::Black, sf::Color(0, 0, 0, 0) };
            auto pix_color = *(colors.begin() + int(classify_char(c, sf::Vector2i(x, y))));
            auto ux = unsigned(x + write_pos.x);
            auto uy = unsigned(y + write_pos.y);
            preren_img.setPixel(ux, uy, pix_color);
            m_font->char_rects[c] = sf::IntRect(write_pos.x, write_pos.y, k_font_dim, k_font_dim);
        }}
        if ((write_pos.x += (k_padding + k_font_dim)) >= text_len) {
            write_pos.y += (k_padding + k_font_dim);
            write_pos.x = 0;
        }
    }
#   if 0
    preren_img.saveToFile("/media/ramdisk/textout.png");
#   endif
    if (!m_font->char_pool.loadFromImage(preren_img)) {
        throw std::runtime_error("TextDrawer::load_internal_font: failed to create font texture.");
    }
}

void TextDrawer::set_text_center(VectorD r, const std::string & text) {
    m_string = text;
    set_text_center(r, std::move(m_string));
#   if 0
    if (!m_font) {
        load_internal_font();
    }

    auto text_width  = float(unsigned(k_font_dim + k_padding)*text.length());
    auto text_height = float(k_font_dim);
    set_text_top_left(r - VectorD(text_width, text_height)*0.5, text);
#   endif
}

void TextDrawer::set_text_top_left(VectorD r, const std::string & text) {
    auto copy = text;
    set_text_top_left(r, std::move(copy));
#   if 0
    if (!m_font) {
        load_internal_font();
    }
    m_string = text;
    m_start_brush.setTexture(m_font->char_pool);
    m_start_brush.setPosition(sf::Vector2f(r));
#   endif
}

void TextDrawer::set_text_center(VectorD r, std::string && text) {
    if (!m_font) {
        load_internal_font();
    }

    auto text_width  = float(unsigned(k_font_dim + k_padding)*text.length());
    auto text_height = float(k_font_dim);
    set_text_top_left(r - VectorD(text_width, text_height)*0.5, std::move(text));
}

void TextDrawer::set_text_top_left(VectorD r, std::string && text) {
    if (!m_font) {
        load_internal_font();
    }
    m_string = std::move(text);
    m_start_brush.setTexture(m_font->char_pool);
    m_start_brush.setPosition(sf::Vector2f(r));
}


void TextDrawer::move(VectorD r) {
    m_start_brush.move(sf::Vector2f(r));
}

std::string TextDrawer::take_string()
    { return std::move(m_string); }

/* private */ void TextDrawer::draw
    (sf::RenderTarget & target, sf::RenderStates states) const
{
    auto brush = m_start_brush;
    for (auto c : m_string) {
        brush.setTextureRect(m_font->char_rects[c]);
        target.draw(brush, states);
        brush.move(float(k_font_dim), 0.f);
    }
}
