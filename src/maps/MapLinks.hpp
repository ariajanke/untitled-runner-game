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

#include <cstdint>

#include <memory>

#include "../Defs.hpp"

enum class MapEdge : uint8_t {
    bottom_right, bottom_left, top_right, top_left,
    right, left, top, bottom,
    not_an_edge
};

class LineMap;
class MapMultiplexerElement;

class MapLinks {
public:
    // right with offset (0, 10)
    // the top most tile appears at (mapwidth, 10)
    // offset "plus" directions
    //   ->
    //  +--------+
    // ||        ||
    // v|        |v
    //  |        |
    //  +--------+
    //   ->
    using MapType      = LineMap;
    using MapPtr       = std::weak_ptr  <const MapType>;
    using MapSharedPtr = std::shared_ptr<const MapType>;

#   if 0
    struct LinkInfo {
        LinkInfo(VectorI offset_, MapSharedPtr mapptr_, MapEdge edge_):
            offset(offset_), mapptr(mapptr_), edge(edge_)
        {}
        VectorI      offset;
        MapSharedPtr mapptr;
        MapEdge      edge  ;
    };

    using MapLinksContainer = std::vector<LinkInfo>;

    void add_links(const MapLinksContainer &);

    void add_link(const LinkInfo &);
#   endif

    void set_dimensions(int width, int height);

    /// @param r must be an edge location
    ///        (x == -1 or y == -1 or x == mapwidth or y == mapheight)
    VectorI translate_edge_tile(VectorI r) const;

    /// @param r must be an edge location
    MapPtr map_at(VectorI r) const;

    void add_link(VectorI offset, MapSharedPtr mapptr, MapEdge edge);

    // I can always yank this out and put it in its own test suite
    static void run_tests();

private:
    static constexpr const int k_uninit = -1;

    struct InterLinkInfo {
        VectorI tile_loc;
        MapPtr  map;
    };

    using InternCont = std::vector<InterLinkInfo>;
    using InternIter = InternCont::iterator;

    struct EdgeRun {
        VectorI beg ;
        VectorI end ;
        VectorI step;
    };

    struct MapTransf {
        // from edge position to other map
        VectorI translation;
        // direction of edge
        VectorI step;
    };

    EdgeRun get_run(MapEdge) const;

    static MapTransf get_transf(MapEdge, int other_width, int other_height);

    int verify_and_get_offset(MapEdge, VectorI) const;

    std::size_t to_index(VectorI r) const;

    int m_width = k_uninit, m_height = k_uninit;
    std::vector<InterLinkInfo> m_link_pointers;
#   if 0
    std::vector<MapPtr> m_pointers_unique;
    bool m_finished = false;
#   endif
};
