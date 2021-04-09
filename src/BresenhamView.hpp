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

// More information about this algorithm can be found here:
// https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
class BresenhamIterator {
public:
    enum { k_jump_to_end };

    BresenhamIterator() {}

    BresenhamIterator(VectorI beg, VectorI last);

    BresenhamIterator(VectorI beg, VectorI last, decltype(k_jump_to_end));

    BresenhamIterator & operator ++ ();

    BresenhamIterator operator ++ (int);

    VectorI operator * () const noexcept { return m_pos; }

    bool operator == (const BresenhamIterator & rhs) const noexcept
        { return is_same(rhs); }

    bool operator != (const BresenhamIterator & rhs) const noexcept
        { return !is_same(rhs); }

private:
    void advance();

    bool is_same(const BresenhamIterator & rhs) const noexcept
        { return m_pos == rhs.m_pos; }

    VectorI step() const noexcept { return m_step; }

    static int norm(int x) { return x <= 0 ? -1 : 1; }

    VectorI m_pos, m_delta, m_step;
    int m_error = 0;
};

class BresenhamView {
public:
    BresenhamView(VectorI beg, VectorI last): m_beg(beg), m_last(last) {}

    BresenhamIterator begin() const
        { return BresenhamIterator(m_beg, m_last); }

    BresenhamIterator end  () const
        { return BresenhamIterator(m_beg, m_last, BresenhamIterator::k_jump_to_end); }

private:
    VectorI m_beg, m_last;
};

// ----------------------------------------------------------------------------

// More information about this algorithm can be found here:
// https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
inline BresenhamIterator::BresenhamIterator(VectorI beg, VectorI last):
    m_pos  (beg),
    m_delta( magnitude(last.x - beg.x), -magnitude(last.y - beg.y) ),
    m_step ( norm(last.x - beg.x), norm(last.y - beg.y) ),
    m_error( m_delta.x + m_delta.y )
{}

inline BresenhamIterator::BresenhamIterator(VectorI beg, VectorI last, decltype(k_jump_to_end)):
    BresenhamIterator(beg, last)
{
    m_pos = last;
    advance();
}

inline BresenhamIterator & BresenhamIterator::operator ++ () {
    advance();
    return *this;
}

inline BresenhamIterator BresenhamIterator::operator ++ (int) {
    auto t = *this;
    advance();
    return t;
}

inline /* private */ void BresenhamIterator::advance() {
    int e2 = m_error*2;
    if (e2 >= m_delta.y) {
        m_error += m_delta.y;
        m_pos.x += step().x;
    }
    if (e2 <= m_delta.x) {
        m_error += m_delta.x;
        m_pos.y += step().y;
    }
}
