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

#include "../Defs.hpp"

#include <memory>

struct LinesView {
public:
    using ValueType = LineSegment;
    using Container = std::vector<ValueType>;
    using Iterator  = std::vector<ValueType>::const_iterator;

    LinesView() {}
    LinesView(Iterator beg_, Iterator end_): m_beg(beg_), m_end(end_) {}

    operator bool() const { return m_beg != m_end; }
    bool operator == (const LinesView & rhs) const { return  is_same_as(rhs); }
    bool operator != (const LinesView & rhs) const { return !is_same_as(rhs); }

    Iterator begin() const { return m_beg; }
    Iterator end  () const { return m_end; }
    std::size_t size() const { return std::size_t(m_end - m_beg); }

private:
    bool is_same_as(const LinesView & rhs) const
        { return rhs.m_beg == m_beg && rhs.m_end == m_end; }
    Iterator m_beg, m_end;
};

// ----------------------------------------------------------------------------

constexpr const char * k_line_map_transition_object = "layer-transition";

class LineMapLoader;

class LineMapLayer final {
public:
    using SegmentsPtr       = std::shared_ptr<std::vector<LineSegment>>;
    using SurfaceDetailsPtr = std::shared_ptr<std::vector<SurfaceDetails>>;

    Surface operator () (const VectorI &, int) const;

    int get_segment_count(const VectorI &) const;

    void load_map_from(LineMapLoader &, Layer);
    void make_blank_of_size(int width, int height);

    double tile_width () const { return m_tile_width ; }
    double tile_height() const { return m_tile_height; }

    int height() const { return m_segments_grid.height(); }
    int width () const { return m_segments_grid.width (); }

    /// translates tile location to pixel location even if that tile location
    /// is out of the map's boundaries
    VectorD get_pixel_offset(VectorI) const;

    VectorI limit_to(VectorI) const;

    bool has_position(VectorI r) const noexcept
        { return m_segments_grid.has_position(r); }
#   if 0
    VectorI tile_location_of(VectorD) const;

    bool is_edge_tile(VectorI) const;
#   endif
    void set_translation(VectorD r)
        { m_translation_to_global = r; }

private:
    void check_invarients() const;

    SegmentsPtr m_segments;
    SurfaceDetailsPtr m_details_ptr;

    // addresses live in m_segments
    Grid<LinesView> m_segments_grid;

    // addresses live in m_details_ptr
    Grid<const SurfaceDetails *> m_surface_details;

    double m_tile_width = 0., m_tile_height = 0.;

    VectorD m_translation_to_global;
};

// ----------------------------------------------------------------------------

namespace tmap { class TiledMap; }

enum class TransitionTileType : uint8_t {
    no_transition,
    toggle_layers,
    to_background,
    to_foreground
};
using TransitionGrid = Grid<TransitionTileType>;

class LineMap final {
public:
    Surface operator ()(Layer, const VectorI &, int) const;

    int get_segment_count(Layer, const VectorI &) const;

    const LineMapLayer & get_layer(const Layer &) const;

    void load_map_from(const tmap::TiledMap &);
    void make_blank_of_size(int width, int height);

    bool tile_in_transition(VectorI) const;
#   if 0
    [[deprecated]] bool point_in_transition(VectorI) const;
#   endif
    bool point_in_transition(VectorD) const;

    double tile_height() const { return m_foreground.tile_height(); }
    double tile_width () const { return m_foreground.tile_width (); }

    int height() const { return m_foreground.height(); }
    int width () const { return m_foreground.width (); }

    VectorI limit_to(VectorI) const;

    void set_translation(VectorD r) {
        m_foreground.set_translation(r);
        m_background.set_translation(r);
    }
private:
    LineMapLayer m_foreground;
    LineMapLayer m_background;

    TransitionGrid m_transition_tiles;
};

// ----------------------------------------------------------------------------

template <typename T>
VectorI limit_to(VectorI r, const Grid<T> & grid) {
    return VectorI(std::min(std::max(r.x, 0), grid.width () - 1),
                   std::min(std::max(r.y, 0), grid.height() - 1));

}

inline std::size_t ref_to_index(const LinesView & view, const LinesView::ValueType & ref) {
    static constexpr const char * k_not_belong_msg =
        "ref does not belong to the container";
    if (view.size() == 0) { throw std::invalid_argument(k_not_belong_msg); }
    auto * beg = &*view.begin();
    auto * end = beg + view.size();
    if (&ref < beg || &ref >= end) { throw std::invalid_argument(k_not_belong_msg); }
    return std::size_t(&ref - beg);
}
