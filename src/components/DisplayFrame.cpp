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

#include "DisplayFrame.hpp"
#include "Defs.hpp"

#include <SFML/Graphics/Sprite.hpp>

#include <common/StringUtil.hpp>

#include <fstream>
#include <sstream>

#include <cassert>
#include <cmath>

namespace {

inline bool is_ss_sep(char c) { return c == ';'; }
inline bool is_ss_nl (char c) { return c == '\n'; }

sf::IntRect read_rect(const char * beg, const char * end);

} // end of <anonymous> namespace

void SpriteSheet::load_from_file(const char * filename) {
#   if 0
    std::ifstream fin;
    fin.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fin.open(filename);
    if (fin.bad() || fin.fail()) {
        throw std::runtime_error("failed to open sheet data");
    }
    std::stringstream sstrm;
    sstrm << fin.rdbuf();
    std::string filecontents = sstrm.str();
#   endif
    std::string filecontents = load_string_from_file(filename);
    bool is_first_line = true;
    const char * fbeg = &filecontents.front();
    const char * fend = fbeg + filecontents.size();
    for_split<is_ss_nl>(fbeg, fend, [&is_first_line, this](const char * beg, const char * end) {
        if (is_first_line) {
            is_first_line = false;
            if (!m_texture.loadFromFile(std::string(beg, end))) {
                throw std::runtime_error("failed to load texture.");
            }
            return;
        }
        bool is_first_term = true;
        for_split<is_ss_sep>(beg, end, [&is_first_term, this](const char * beg, const char * end) {
            if (is_first_term) {
                is_first_term = false;
                m_seq_offsets.push_back(int(m_frames.size()));
            }
            m_frames.push_back(read_rect(beg, end));
        });
    });

    m_seq_offsets.push_back(int(m_frames.size()));
    if (!m_seq_offsets.empty()) {
        assert(m_seq_offsets.size() > 1);
        for (std::size_t i = 1; i != m_seq_offsets.size(); ++i) {
            assert(m_seq_offsets[i - 1] != m_seq_offsets[i]);
        }
    }
}

void SpriteSheet::bind_to
    (sf::Sprite & spt, int sequence_number, double sequence_time) const
{
    if (m_seq_offsets.empty()) {
        throw std::runtime_error("uninit ss");
    }
    if (sequence_number < 0 || sequence_number >= int(m_seq_offsets.size())) {
        throw std::invalid_argument("bad seq num");
    }
    if (sequence_time < 0. || sequence_time >= 1.) {
        throw std::invalid_argument("bad seq time");
    }
    auto seq_off = m_seq_offsets[std::size_t(sequence_number)];
    auto seq_len = m_seq_offsets[std::size_t(sequence_number + 1)] - seq_off;
    auto pos = int(std::round(double(seq_len)*sequence_time));
    bind_to(spt, sequence_number, int(pos));
}

void SpriteSheet::bind_to(sf::Sprite & sprite, int sequence_number, int sequence_frame) const {
    auto seq_off = m_seq_offsets[std::size_t(sequence_number)];
    sprite.setTexture(m_texture);
    sprite.setTextureRect(m_frames[std::size_t(seq_off + sequence_frame)]);
}

int SpriteSheet::next_frame(int sequence_number, int frame_number) const noexcept {
    auto seq_off = m_seq_offsets[std::size_t(sequence_number)];
    auto seq_len = m_seq_offsets[std::size_t(sequence_number + 1)] - seq_off;
    return (frame_number + 1) % seq_len;
}

const sf::IntRect & SpriteSheet::frame
    (int sequence_number, int sequence_frame) const
{
    auto seq_off = m_seq_offsets.at(std::size_t(sequence_number));
    return m_frames.at(std::size_t(seq_off + sequence_frame));
}

std::size_t SpriteSheet::total_frame_count() const noexcept
    { return m_frames.size(); }

/* static */ sf::IntRect SpriteSheet::parse_rect
    (const char * beg, const char * end)
{ return read_rect(beg, end); }

/* static */ std::string SpriteSheet::load_string_from_file(const char * filename) {
    std::ifstream fin;
    fin.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fin.open(filename);
    if (fin.bad() || fin.fail()) {
        throw std::runtime_error("failed to file");
    }
    std::stringstream sstrm;
    sstrm << fin.rdbuf();
    return sstrm.str();
}

namespace {
#if 0
inline bool is_ss_param(char c) { return c == ','; }
#endif
sf::IntRect read_rect(const char * beg, const char * end) {
    sf::IntRect rect;
    auto list = { &rect.left, &rect.top, &rect.width, &rect.height };
    auto itr = list.begin();
    auto endf = list.end();
    static const char * const k_must_be_exactly_4 = "There must be exactly four arguments for rectangle.";
    for_split<is_comma>(beg, end, [&itr, endf](const char * beg, const char * end) {
        if (itr == endf) {
            throw std::runtime_error(k_must_be_exactly_4);
        }
        if (!string_to_number(beg, end, **itr++)) {
            throw std::runtime_error("Must be numeric");
        }
    });
    if (itr != endf) {
        throw std::runtime_error(k_must_be_exactly_4);
    }
    return rect;
}

} // end of <anonymous> namespace
