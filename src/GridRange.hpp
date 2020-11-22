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

#include <common/Grid.hpp>

template <bool t_is_const, typename T>
class GridIterator {
public:
    using ContainerType    = typename std::conditional<t_is_const, const Grid<T>, Grid<T>>::type;
    using ElementReference = typename std::conditional<t_is_const, typename std::vector<T>::const_reference, typename std::vector<T>::reference>::type;
    using ElementPointer   = typename std::conditional<t_is_const, typename std::vector<T>::const_pointer, typename std::vector<T>::pointer>::type;

    GridIterator() = default;
    GridIterator(ContainerType & parent_, int x_min_, int x_max_, int y_):
        x(x_min_), y(y_), x_min(x_min_), x_max(x_max_), parent(&parent_)
    {}

    ElementReference operator * () const { return (*parent)(x, y); }
    ElementPointer operator -> () const { return &(*parent)(x, y); }

    void operator ++ () {
        if ((++x % x_max) == 0) {
            ++y;
            x = x_min;
        }
    }

    bool is_equal_to(const GridIterator & rhs) const {
        if (parent == rhs.parent)
            return x == rhs.x && y == rhs.y;
        throw std::invalid_argument("GridIterator::is_equal_to: cannot compare iterators with different parents.");
    }

    bool operator == (const GridIterator & rhs) const { return  is_equal_to(rhs); }
    bool operator != (const GridIterator & rhs) const { return !is_equal_to(rhs); }

    explicit operator sf::Vector2i() const noexcept { return sf::Vector2i(x, y); }

private:
    int x = 0, y = 0, x_min = 0, x_max = 0;
    ContainerType * parent = nullptr;
};

template <bool t_is_const, typename T>
class GridRangeImpl {
public:
    using ContainerType    = typename GridIterator<t_is_const, T>::ContainerType;
    using ElementReference = typename GridIterator<t_is_const, T>::ElementReference;
    using ElementPointer   = typename GridIterator<t_is_const, T>::ElementPointer;

    GridRangeImpl() = default;

    GridRangeImpl(ContainerType & parent_, int x_beg_, int y_beg_, int x_end_, int y_end_):
         x_beg(x_beg_), y_beg(y_beg_), x_end(x_end_), y_end(y_end_), parent(&parent_)
    {}

    GridIterator<t_is_const, T> begin() const {
        return GridIterator<t_is_const, T>(*parent, x_beg, x_end, y_beg);
    }

    GridIterator<t_is_const, T> end() const {
        return GridIterator<t_is_const, T>(*parent, x_beg, x_end, y_end);
    }

    sf::Vector2i begin_position() const noexcept
        { return sf::Vector2i(x_beg, y_beg); }

    sf::Vector2i last_position() const noexcept
        { return sf::Vector2i(x_end - 1, y_end - 1); }
private:
    int x_beg = 0, y_beg = 0, x_end = 0, y_end = 0;
    ContainerType * parent = nullptr;
};

template <typename T>
using GridRange = GridRangeImpl<false, T>;

template <typename T>
using ConstGridRange = GridRangeImpl<true, T>;
