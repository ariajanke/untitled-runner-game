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

#include "Maps.hpp"
#include "LineMapLoader.hpp"

#include <common/TestSuite.hpp>

#include <cassert>

namespace {

using Error = std::runtime_error;
using InvArg = std::invalid_argument;

template <typename Type>
VectorI limit_vector_to(const Grid<Type> & grid, VectorI r) {
    return VectorI(std::max(std::min(r.x, grid.width () - 1), 0),
                   std::max(std::min(r.y, grid.height() - 1), 0));
}

} // end of <anonymous> namespace

Surface LineMapLayer::operator ()(const VectorI & tile_loc, int segnum) const {
    static constexpr const char * const k_tile_loc_oor_msg =
        "LineMapLayer::operator(): tile_loc is out of range.";

    auto view = m_segments_grid(tile_loc);
    if (segnum < 0 || segnum >= int(view.size()))
        throw std::out_of_range(k_tile_loc_oor_msg);

    auto seg = *(view.begin() + segnum);
    auto offset = get_pixel_offset(tile_loc);
    seg.a += offset;
    seg.b += offset;

    return Surface { seg, *m_surface_details(tile_loc) };
}

int LineMapLayer::get_segment_count(const VectorI & tile_location) const {
    if (!m_segments_grid.has_position(tile_location)) return 0;
    return int(m_segments_grid(tile_location).size());
}

void LineMapLayer::load_map_from(LineMapLoader & map_loader, Layer layer) {
    map_loader.load_layer_into(m_segments_grid, m_surface_details, layer);
    m_segments = map_loader.get_segments();
    m_tile_width = map_loader.tile_width();
    m_tile_height = map_loader.tile_height();
    check_invarients();
}

void LineMapLayer::make_blank_of_size(int width_, int height_) {
    static const LinesView::Container blank_cont;
    m_segments_grid.set_size(width_, height_, LinesView(blank_cont.begin(), blank_cont.end()));
    m_surface_details.set_size(width_, height_, nullptr);
    check_invarients();
}

VectorD LineMapLayer::get_pixel_offset(VectorI r) const {
    return VectorD(double(r.x)*tile_width(), double(r.y)*tile_height()) +
           m_translation_to_global;
}

VectorI LineMapLayer::limit_to(VectorI r) const {
    return limit_vector_to(m_segments_grid, r);
}

#if 0
VectorI LineMapLayer::tile_location_of(VectorD r) const {

}

bool LineMapLayer::is_edge_tile(VectorI r) const {

}
#endif

/* private */ void LineMapLayer::check_invarients() const {
    assert(m_segments_grid.height() == m_surface_details.height() &&
           m_segments_grid.width () == m_surface_details.width ());
}

// ----------------------------------------------------------------------------

Surface LineMap::operator ()(Layer layer, const VectorI & tile_loc, int segnum) const
    { return get_layer(layer)(tile_loc, segnum); }

int LineMap::get_segment_count(Layer layer, const VectorI & r) const
    { return get_layer(layer).get_segment_count(r); }

const LineMapLayer & LineMap::get_layer(const Layer & layer) const {
    switch (layer) {
    case Layer::background: return m_background;
    case Layer::foreground: return m_foreground;
    default: break;
    }
    throw std::invalid_argument("LineMapN::get_layer: layer must be either foreground or background.");
}

void LineMap::load_map_from(const tmap::TiledMap & tlmap) {
    LineMapLoader lml;
    lml.load_map(tlmap);
    m_foreground.load_map_from(lml, Layer::foreground);
    m_background.load_map_from(lml, Layer::background);
    lml.load_transitions_into(m_transition_tiles);
}

void LineMap::make_blank_of_size(int width_, int height_) {
    m_foreground.make_blank_of_size(width_, height_);
    m_background.make_blank_of_size(width_, height_);
}

bool LineMap::tile_in_transition(VectorI r) const {
    if (!m_transition_tiles.has_position(r)) return false;
    return m_transition_tiles(r) != TransitionTileType::no_transition;
}
#if 0
bool LineMap::point_in_transition(VectorI r) const {
    return tile_in_transition(r);
}
#endif
bool LineMap::point_in_transition(VectorD point) const {
    auto twidth  = m_foreground.tile_width ();
    auto theight = m_foreground.tile_height();

    VectorI r(int(std::floor(point.x / twidth)), int(std::floor(point.y / theight)));
    return tile_in_transition(r);
}

VectorI LineMap::limit_to(VectorI r) const {
    return m_foreground.limit_to(r);
}
