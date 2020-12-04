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

#include "Maps.hpp"

#include <memory>

class LineMapLoader {
public:
    using SegmentsPtr        = LineMapLayer::SegmentsPtr;
    using SurfaceDetailsPtr  = LineMapLayer::SurfaceDetailsPtr;
    using SurfaceDetailsGrid = Grid<const SurfaceDetails *>;

    static constexpr const char * k_ground     = "ground"    ;
    static constexpr const char * k_background = "background";
    static constexpr const char * k_foreground = "foreground";
    static constexpr const auto k_layer_list =
        { k_ground, k_background, k_foreground };

    static constexpr const char * k_transition_object = k_line_map_transition_object;
    static constexpr const double k_initial_tile_size = -1.;

    void load_map(const tmap::TiledMap &);

    /// meant to be called only by LineMap
    void load_layer_into(Grid<LinesView> &, SurfaceDetailsGrid &, Layer);

    void load_transitions_into(TransitionGrid &);

    SegmentsPtr get_segments() const { return m_segments; }

    SurfaceDetailsPtr get_surface_details() const { return m_details; }

    double tile_width() const { return m_tile_width; }

    double tile_height() const { return m_tile_height; }

    // ------------------- not relevant to class interface --------------------

    struct TileInfo final : public SurfaceDetails {
        std::vector<LineSegment> segments;
    };

    using SegmentMap        = std::unordered_map<int, TileInfo >;
    using LineViewMap       = std::unordered_map<int, LinesView>;
    using SurfaceDetailsMap = std::unordered_map<int, const SurfaceDetails *>;

    struct SegmentsInfo {
        SegmentMap segment_map;
        int total_segments_count = 0;
    };

private:
    void load_tile_size(const tmap::TiledMap &);

    SegmentsInfo load_tileset_map(const tmap::TiledMap &) const;

    void load_transition_tiles(const tmap::TiledMap &, TransitionGrid &) const;

    bool has_tile_size_initialized() const noexcept {
        return    m_tile_width  != k_initial_tile_size
               && m_tile_height != k_initial_tile_size;
    }

    Grid<LinesView> m_foreground, m_background;
    SurfaceDetailsGrid m_foreground_details, m_background_details;
    TransitionGrid m_transition_tiles;
    double m_tile_width = k_initial_tile_size;
    double m_tile_height = k_initial_tile_size;
    SegmentsPtr m_segments;
    SurfaceDetailsPtr m_details;
};
