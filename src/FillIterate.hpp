#pragma once

#include <common/SubGrid.hpp>

template <typename T, typename IsInGroupFunc, typename DoFunc>
void iterate_grid_group(const Grid<T> &, IsInGroupFunc &&, DoFunc &&);

template <bool k_is_const, typename T, typename IsInGroupFunc, typename DoFunc>
void iterate_grid_group(SubGridImpl<k_is_const, T>, IsInGroupFunc &&, DoFunc &&);

template <bool k_is_const, typename T, typename IsInGroupFunc, typename DoFunc>
void iterate_grid_group(SubGridImpl<k_is_const, T>, SubGrid<bool> explored,
                        IsInGroupFunc &&, DoFunc &&);

template <bool k_is_const, typename T, typename IsInGroupFunc, typename DoFunc>
void iterate_grid_group(SubGridImpl<k_is_const, T>, sf::Vector2i,
                        IsInGroupFunc &&, DoFunc &&);

// ----------------------------------------------------------------------------

template <typename T, std::size_t k_size, typename Func>
std::array<T, k_size> transform(const std::array<T, k_size> & array, Func && f) {
    auto rv = array;
    std::transform(array.begin(), array.end(), rv.begin(), std::move(f));
    return rv;
}

template <bool k_is_const, typename T, typename IsInGroupFunc, typename DoFunc>
void iterate_grid_group_helper
    (SubGridImpl<k_is_const, T> space, SubGrid<bool> explored, sf::Vector2i from,
     const IsInGroupFunc & is_in_group, const DoFunc & do_f);

template <typename T, typename IsInGroupFunc, typename DoFunc>
void iterate_grid_group(const Grid<T> & grid, IsInGroupFunc && is_in_group, DoFunc && do_f)
    { iterate_grid_group(make_sub_grid(grid), std::move(is_in_group), std::move(do_f)); }

template <bool k_is_const, typename T, typename IsInGroupFunc, typename DoFunc>
void iterate_grid_group(SubGridImpl<k_is_const, T> space, IsInGroupFunc && is_in_group, DoFunc && do_f) {
    Grid<bool> temp;
    temp.set_size(space.width(), space.height(), false);
    iterate_grid_group(space, temp, std::move(is_in_group), std::move(do_f));
}

template <bool k_is_const, typename T, typename IsInGroupFunc, typename DoFunc>
void iterate_grid_group
    (SubGridImpl<k_is_const, T> space, SubGrid<bool> explored,
     IsInGroupFunc && is_in_group, DoFunc && do_f)
{
    for (sf::Vector2i r; r != space.end_position(); r = space.next(r)) {
        if (!is_in_group(r) || explored(r)) continue;
        do_f(r, true); // signal group start
        explored(r) = true;
        iterate_grid_group_helper(space, explored, r, is_in_group, do_f);
    }
}

template <bool k_is_const, typename T, typename IsInGroupFunc, typename DoFunc>
void iterate_grid_group(SubGridImpl<k_is_const, T> space, sf::Vector2i r,
                        IsInGroupFunc && is_in_group, DoFunc && do_f)
{
    Grid<bool> temp;
    temp.set_size(space.width(), space.height(), false);
    temp(r) = true;
    do_f(r, true);
    iterate_grid_group_helper(space, temp, r, is_in_group, do_f);
}

template <bool k_is_const, typename T, typename IsInGroupFunc, typename DoFunc>
void iterate_grid_group_helper
    (SubGridImpl<k_is_const, T> space, SubGrid<bool> explored, sf::Vector2i from,
     const IsInGroupFunc & is_in_group, const DoFunc & do_f)
{
    // assume "from" has already been handled
    //assert(explored(from));
    // lets try an iterative approach

    using VectorI = sf::Vector2i;
    static const std::array k_neighbor_offsets = {
        VectorI(0, 1), VectorI(0, -1), VectorI(1,  0), VectorI(-1,  0),
        //VectorI(1, 1), VectorI(1, -1), VectorI(1, -1), VectorI(-1, -1)
    };
    static auto push_offsets_from = [](std::vector<VectorI> & pos, VectorI r) {
        auto neighbors = transform(k_neighbor_offsets, [r](VectorI t) { return t + r; });
        pos.insert(pos.end(), neighbors.begin(), neighbors.end());
    };
    std::vector<VectorI> offsets;
    offsets.reserve(space.width()*space.height());
    push_offsets_from(offsets, from);
    while (!offsets.empty()) {
        auto t = offsets.back();
        offsets.pop_back();
        if (!space.has_position(t)) continue;
        if (!is_in_group(t) || explored(t)) continue;
        do_f(t, false);
        explored(t) = true;
        push_offsets_from(offsets, t);
        if (offsets.size() > space.width()*space.height()*k_neighbor_offsets.size()) {
            throw std::invalid_argument("safety throw... offsets container was not meant to grow this large.");
        }
    }
}
