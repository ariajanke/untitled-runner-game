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
#include <common/SubGrid.hpp>
#if 0
template <bool k_is_const_t, typename T>
class SubGridImpl {
    struct Dummy {};
public:
    friend class SubGridImpl<!k_is_const_t, T>;
    using ParentPointer      = typename std::conditional<k_is_const_t, const Grid<T> *, Grid<T> *>::type;
    using ParentReference    = typename std::conditional<k_is_const_t, const Grid<T> &, Grid<T> &>::type;
    using Element            = typename Grid<T>::Element;
    using Reference          = typename Grid<T>::ReferenceType;
    using ConstReference     = typename Grid<T>::ConstReferenceType;
    using VectorI            = sf::Vector2i;

    static constexpr const int  k_rest_of_grid = -1;
    static constexpr const bool k_is_const     = k_is_const_t;

    SubGridImpl() {}
    SubGridImpl(typename std::conditional<k_is_const, const SubGridImpl<false, T> &, Dummy>::type);

    SubGridImpl(const SubGridImpl &) = default;
    SubGridImpl(SubGridImpl &&) = default;

    SubGridImpl(ParentReference,
                int width_ = k_rest_of_grid, int height_ = k_rest_of_grid);
    SubGridImpl(ParentReference, VectorI offset,
                int width_ = k_rest_of_grid, int height_ = k_rest_of_grid);

    SubGridImpl & operator = (const SubGridImpl &) = default;
    SubGridImpl & operator = (SubGridImpl &&) = default;

    const Grid<T> & parent() const { return *m_parent; }

    template <bool k_is_const_ = k_is_const_t>
    typename std::enable_if<!k_is_const_, Reference>::type operator () (const VectorI & r) { return element(r.x, r.y); }

    ConstReference operator () (const VectorI & r) const { return element(r.x, r.y); }

    template <bool k_is_const_ = k_is_const_t>
    typename std::enable_if<!k_is_const_, Reference>::type operator () (int x, int y) { return element(x, y); }

    ConstReference operator () (int x, int y) const { return element(x, y); }

    std::size_t size() const noexcept { return std::size_t(m_width*m_height); }

    bool is_empty() const noexcept { return m_width != 0 && m_height != 0; }

    /** @return grid width in number of elements */
    int width() const noexcept { return m_width; }

    /** @return grid height in number of elements */
    int height() const noexcept { return m_height; }

    /** @returns true if position is inside the grid */
    bool has_position(int x, int y) const noexcept;

    /** @returns true if position is inside the grid */
    bool has_position(const sf::Vector2i & r) const noexcept
        { return has_position(r.x, r.y); }

    sf::Vector2i next(const sf::Vector2i &) const noexcept;

    sf::Vector2i end_position() const noexcept;

    SubGridImpl<true, T> make_sub_grid
        (int width = k_rest_of_grid, int height = k_rest_of_grid) const;

    SubGridImpl<true, T> make_sub_grid
        (VectorI offset, int width = k_rest_of_grid, int height = k_rest_of_grid) const;

    template <bool k_is_const_ = k_is_const_t>
    typename std::enable_if<!k_is_const_, SubGridImpl<false, T>>::type make_sub_grid
        (int width = k_rest_of_grid, int height = k_rest_of_grid);

    template <bool k_is_const_ = k_is_const_t>
    typename std::enable_if<!k_is_const_, SubGridImpl<false, T>>::type make_sub_grid
        (VectorI offset, int width = k_rest_of_grid, int height = k_rest_of_grid);

    bool sub_grid_will_fit
        (VectorI offset,
         int width = k_rest_of_grid, int height = k_rest_of_grid) const noexcept;

    void swap(SubGridImpl<k_is_const_t, T> &) noexcept;
private:
    template <bool k_is_const_ = k_is_const_t>
    typename std::enable_if<!k_is_const_, Reference>::type
        element(int x, int y)
    {
        verify_position_ok(x, y);
        return (*m_parent)(x + m_offset.x, y + m_offset.y);
    }

    ConstReference element(int x, int y) const {
        verify_position_ok(x, y);
        return (*m_parent)(x + m_offset.x, y + m_offset.y);
    }

    void verify_position_ok(int x, int y) const;

    void verify_sub_grid_will_fit(VectorI offset, int width_, int height_) const;

    void verify_invarients() const;

    static VectorI verify_offset(ParentReference, VectorI);

    static int verify_size(int max, int size, const char * name);

    VectorI m_offset;
    int m_width = 0;
    int m_height = 0;
    ParentPointer m_parent = nullptr;
};

template <typename T>
using SubGrid = SubGridImpl<false, T>;

template <typename T>
using ConstSubGrid = SubGridImpl<true, T>;

// ----------------------------------------------------------------------------

template <bool k_is_const_t, typename T>
SubGridImpl<k_is_const_t, T>::SubGridImpl
    (typename std::conditional<k_is_const, const SubGridImpl<false, T> &, Dummy>::type rhs):
    m_offset(rhs.m_offset),
    m_width (rhs.m_width ),
    m_height(rhs.m_height),
    m_parent(rhs.m_parent)
{
    verify_invarients();
}

template <bool k_is_const_t, typename T>
SubGridImpl<k_is_const_t, T>::SubGridImpl
    (ParentReference parent, int width_, int height_):
    SubGridImpl(parent, VectorI(), width_, height_)
{
    verify_invarients();
}

template <bool k_is_const_t, typename T>
SubGridImpl<k_is_const_t, T>::SubGridImpl
    (ParentReference parent, VectorI offset, int width_, int height_):
    m_offset(verify_offset(parent, offset)),
    m_width (verify_size(parent.width () - offset.x, width_ , "width" )),
    m_height(verify_size(parent.height() - offset.y, height_, "height")),
    m_parent(&parent)
{
    verify_invarients();
}

template <bool k_is_const_t, typename T>
bool SubGridImpl<k_is_const_t, T>::has_position(int x, int y) const noexcept {
    return (x >= 0 && y >= 0 && x < width() && y < height());
}

template <bool k_is_const_t, typename T>
sf::Vector2i SubGridImpl<k_is_const_t, T>::next
    (const sf::Vector2i & r) const noexcept
{
    auto rv = r;
    if (++rv.x == width()) {
        ++rv.y;
        rv.x = 0;
    }
    return rv;
}

template <bool k_is_const_t, typename T>
sf::Vector2i SubGridImpl<k_is_const_t, T>::end_position() const noexcept
    { return VectorI(0, height()); }

template <bool k_is_const_t, typename T>
SubGridImpl<true, T> SubGridImpl<k_is_const_t, T>::make_sub_grid
    (int width_, int height_) const
    { return make_sub_grid(parent(), VectorI(), width_, height_); }

template <bool k_is_const_t, typename T>
SubGridImpl<true, T> SubGridImpl<k_is_const_t, T>::
    make_sub_grid(VectorI offset, int width_, int height_) const
{
    SubGridImpl<true, T> rv;
    verify_sub_grid_will_fit(offset, width_, height_);
    rv.m_parent = m_parent;
    rv.m_offset = m_offset + offset;
    rv.m_width  = width_  == k_rest_of_grid ? width () - offset.x : width_ ;
    rv.m_height = height_ == k_rest_of_grid ? height() - offset.y : height_;
    verify_invarients();
    return rv;
}

template <bool k_is_const_t, typename T>
template <bool k_is_const_>
typename std::enable_if<!k_is_const_, SubGridImpl<false, T>>::type
    SubGridImpl<k_is_const_t, T>::make_sub_grid
    (int width_, int height_)
    { return make_sub_grid(VectorI(), width_, height_); }

template <bool k_is_const_t, typename T>
template <bool k_is_const_>
typename std::enable_if<!k_is_const_, SubGridImpl<false, T>>::type
    SubGridImpl<k_is_const_t, T>::make_sub_grid
    (VectorI offset, int width_, int height_)
{
    SubGridImpl<k_is_const_, T> rv;
    verify_sub_grid_will_fit(offset, width_, height_);
    rv.m_parent = m_parent;
    rv.m_offset = m_offset + offset;
    rv.m_width  = width_  == k_rest_of_grid ? width () - offset.x : width_ ;
    rv.m_height = height_ == k_rest_of_grid ? height() - offset.y : height_;
    verify_invarients();
    return rv;
}

template <bool k_is_const_t, typename T>
bool SubGridImpl<k_is_const_t, T>::sub_grid_will_fit
    (VectorI offset, int width_, int height_) const noexcept
{
    return
        (offset.x >= 0 && offset.x < width ()) &&
        (offset.y >= 0 && offset.y < height()) &&
        (width_  <= width () - offset.x || width_ == k_rest_of_grid) &&
        (height_ <= height() - offset.y || width_ == k_rest_of_grid);
}

template <bool k_is_const_t, typename T>
void SubGridImpl<k_is_const_t, T>::swap
    (SubGridImpl<k_is_const_t, T> & rhs) noexcept
{
    std::swap(m_offset, rhs.m_offset);
    std::swap(m_width , rhs.m_width );
    std::swap(m_height, rhs.m_height);
    std::swap(m_parent, rhs.m_parent);
    verify_invarients();
}

template <bool k_is_const_t, typename T>
/* private */ void SubGridImpl<k_is_const_t, T>::
    verify_position_ok(int x, int y) const
{
    if (has_position(x, y)) return;
    throw std::out_of_range("Position out of range.");
}

template <bool k_is_const_t, typename T>
/* private */ void SubGridImpl<k_is_const_t, T>::verify_sub_grid_will_fit
    (VectorI offset, int width_, int height_) const
{
    if (sub_grid_will_fit(offset, width_, height_)) return;
    throw std::invalid_argument("Sub grid will not fit.");
}

template <bool k_is_const_t, typename T>
/* private static */ sf::Vector2i SubGridImpl<k_is_const_t, T>::
    verify_offset(ParentReference parent, VectorI offset)
{
    if (parent.has_position(offset)) return offset;
    throw std::out_of_range("Offset not contained in parent.");
}

template <bool k_is_const_t, typename T>
/* private static */ int SubGridImpl<k_is_const_t, T>::verify_size
    (int max, int size, const char * name)
{
    if (size == k_rest_of_grid) return max;
    if (size >= 0 && size <= max) return size;
    throw std::out_of_range("Sub grid " + std::string(name) +
                            " cannot fit inside the parent container.");
}

template <bool k_is_const_t, typename T>
void SubGridImpl<k_is_const_t, T>::verify_invarients() const {
    bool invarients_ok = false;
    if (m_parent) {
        invarients_ok =
            m_offset.x >= 0 && m_offset.x <= m_parent->width () &&
            m_offset.y >= 0 && m_offset.y <= m_parent->height() &&
            m_width  >= 0 && m_offset.x + m_width  <= m_parent->width () &&
            m_height >= 0 && m_offset.y + m_height <= m_parent->height();
    } else {
        invarients_ok = m_width == 0 && m_height == 0 && m_offset == VectorI();
    }
    if (!invarients_ok) {
        throw std::runtime_error("SubGridImpl::verify_invarients(): invarients failed.");
    }
}
#endif
