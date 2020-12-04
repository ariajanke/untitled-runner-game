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

#include "MapLinks.hpp"

#include "Maps.hpp"

#include <common/TestSuite.hpp>

#include <cassert>

namespace {

using Error  = std::runtime_error;
using InvArg = std::invalid_argument;

} // end of <anonymous> namespace

void MapLinks::set_dimensions(int width, int height) {
    m_link_pointers.clear();
    m_width = width;
    m_height = height;
    m_link_pointers.resize(4 + width*2 + height*2);
}

VectorI MapLinks::translate_edge_tile(VectorI r) const
    { return m_link_pointers[to_index(r)].tile_loc; }

MapLinks::MapPtr MapLinks::map_at(VectorI r) const
    { return m_link_pointers[to_index(r)].map; }
#if 0
void MapLinks::add_links(const MapLinksContainer & container) {
    for (const auto & nfo : container) add_link(nfo);
}

void MapLinks::add_link(const LinkInfo & nfo) {
    add_link(nfo.offset, nfo.mapptr, nfo.edge);
}
#endif

void MapLinks::add_link(VectorI offset, MapSharedPtr mapptr, MapEdge edge) {
    if (m_width == k_uninit || m_height == k_uninit) {
        throw Error("MapLinks::update_links: dimensions must be set before updating links.");
    }

    auto run    = get_run(edge);
    auto transf = get_transf(edge, mapptr->width(), mapptr->height());
    auto rem    = verify_and_get_offset(edge, offset);
    for (auto r = run.beg; r != run.end; r += run.step) {
        if (rem > 0) {
            --rem;
            continue;
        }

        auto & link = m_link_pointers[to_index(r)];
        if (!link.map.expired()) {
            throw InvArg("MapLinks::update_links: Provided maps must not overlap at the edges.");
        }

        link.map      = mapptr;
        link.tile_loc = transf.translation + r;

        transf.translation += transf.step;
    }
}

/* static */ void MapLinks::run_tests() {
    ts::TestSuite suite;
    suite.start_series("MapLinks");
    suite.test([]() {
        MapLinks ml;
        ml.set_dimensions( 4,  3);
        ml.map_at(VectorI( 4,  3));
        ml.map_at(VectorI(-1, -1));
        ml.map_at(VectorI( 4, -1));
        ml.map_at(VectorI(-1,  3));

        ml.map_at(VectorI(-1,  2));
        ml.map_at(VectorI( 4,  2));

        ml.map_at(VectorI( 2, -1));
        ml.map_at(VectorI( 3, -1));
        return ts::test(true);
    });
    static const auto mk_single_map_test = [](int w, int h, MapEdge edge) {
        auto shrmap = std::make_shared<LineMap>();
        shrmap->make_blank_of_size(w, h);
        MapLinks links;
        links.set_dimensions(w, h);
        links.add_link(VectorI(), shrmap, edge);
        return std::make_pair(shrmap, links);
    };
    suite.test([]() {
        auto [shrmap, links] = mk_single_map_test(3, 3, MapEdge::top);
        (void)shrmap;
        return ts::test(
            links.map_at(VectorI(-1, -1)).expired() ||
           !links.map_at(VectorI( 0, -1)).expired() ||
           !links.map_at(VectorI( 1, -1)).expired() ||
           !links.map_at(VectorI( 2, -1)).expired() ||
            links.map_at(VectorI( 3, -1)).expired());
    });
    suite.test([]() {
        auto [shrmap, ml] = mk_single_map_test(3, 4, MapEdge::left);
        (void)shrmap;
        return ts::test(
             ml.map_at(VectorI(-1, -1)).expired() ||
            !ml.map_at(VectorI(-1,  0)).expired() ||
            !ml.map_at(VectorI(-1,  1)).expired() ||
            !ml.map_at(VectorI(-1,  2)).expired() ||
            !ml.map_at(VectorI(-1,  3)).expired() ||
             ml.map_at(VectorI(-1,  4)).expired());
    });
    suite.test([]() {
        auto [shrmap, ml] = mk_single_map_test(2, 3, MapEdge::bottom_right);
        (void)shrmap;
        return ts::test(
             ml.map_at(VectorI(1, 3)).expired() ||
            !ml.map_at(VectorI(2, 3)).expired() ||
             ml.map_at(VectorI(2, 2)).expired() ||
             ml.translate_edge_tile(VectorI(2, 3)) == VectorI(0, 0));
    });
    // linked to multiple (after this, we're done with checking pointer holes!)
    suite.test([]() {
        auto map1 = std::make_shared<LineMap>();
        auto map2 = std::make_shared<LineMap>();
        MapLinks links;
        links.set_dimensions(3, 3);
        map1->make_blank_of_size(2, 2);
        map2->make_blank_of_size(2, 4);
        links.add_link(VectorI(), map1, MapEdge::bottom);
        links.add_link(VectorI(), map2, MapEdge::right );
        bool map1_links_ok = links.map_at(VectorI(-1, 3)).expired() &&
                             links.map_at(VectorI( 0, 3)).lock() == map1 &&
                             links.map_at(VectorI( 1, 3)).lock() == map1 &&
                             links.map_at(VectorI( 2, 3)).expired();
        bool map2_links_ok = links.map_at(VectorI(3, -1)).expired() &&
                             links.map_at(VectorI(3,  0)).lock() == map2 &&
                             links.map_at(VectorI(3,  1)).lock() == map2 &&
                             links.map_at(VectorI(3,  2)).lock() == map2 &&
                             links.map_at(VectorI(3,  3)).lock() == map2;
        return ts::test(map1_links_ok && map2_links_ok);
    });
    // multiple with ok translations
    //suite.do_test([]() {

    //});
    // fill links
    // overlap
    // positive offsets
    // negative offsets
    // multi offsets
    suite.finish_up();
}

/* private */ MapLinks::EdgeRun MapLinks::get_run(MapEdge edge) const {
    using Mp  = MapEdge;
    using Vec = VectorI;
    switch (edge) {
    case Mp::bottom_left : return EdgeRun { Vec(     -1, m_height), Vec(          0, m_height), Vec(1, 0) };
    case Mp::bottom_right: return EdgeRun { Vec(m_width, m_height), Vec(m_width + 1, m_height), Vec(1, 0) };
    case Mp::top_left    : return EdgeRun { Vec(     -1,       -1), Vec(          0,       -1), Vec(1, 0) };
    case Mp::top_right   : return EdgeRun { Vec(m_width,       -1), Vec(m_width + 1,       -1), Vec(1, 0) };
    case Mp::left        : return EdgeRun { Vec(     -1,        0), Vec(         -1, m_height), Vec(0, 1) };
    case Mp::right       : return EdgeRun { Vec(m_width,        0), Vec(    m_width, m_height), Vec(0, 1) };
    case Mp::top         : return EdgeRun { Vec(      0,       -1), Vec(    m_width,       -1), Vec(1, 0) };
    case Mp::bottom      : return EdgeRun { Vec(      0, m_height), Vec(    m_width, m_height), Vec(1, 0) };
    default: throw BadBranchException();
    }
}

/* static private */ MapLinks::MapTransf MapLinks::get_transf
    (MapEdge edge, int owidth, int oheight)
{
    using Mp  = MapEdge;
    using Vec = VectorI;
    switch (edge) {
    // corners may not have offsets
    case Mp::bottom_left : return MapTransf { Vec(owidth - 1,           0), Vec() };
    case Mp::bottom_right: return MapTransf { Vec(         0,           0), Vec() };
    case Mp::top_left    : return MapTransf { Vec(owidth - 1, oheight - 1), Vec() };
    case Mp::top_right   : return MapTransf { Vec(         0, oheight - 1), Vec() };
    case Mp::left        : return MapTransf { Vec(owidth - 1,           0), Vec(0, 1) };
    case Mp::right       : return MapTransf { Vec(         0,           0), Vec(0, 1) };
    case Mp::top         : return MapTransf { Vec(         0, oheight - 1), Vec(1, 0) };
    case Mp::bottom      : return MapTransf { Vec(         0,           0), Vec(1, 0) };
    default: throw BadBranchException();
    }
}

/* private */ int MapLinks::verify_and_get_offset(MapEdge edge, VectorI offset) const {
    using Mp = MapEdge;
    switch (edge) {
    // corners may not have offsets
    case Mp::bottom_left :
    case Mp::bottom_right:
    case Mp::top_left    :
    case Mp::top_right   : assert(offset == VectorI()); return 0;
    case Mp::left        :
    case Mp::right       : assert(offset.x == 0); return offset.y;
    case Mp::top         :
    case Mp::bottom      : assert(offset.y == 0); return offset.x;
    default: throw BadBranchException();
    }
}

/* private */ std::size_t MapLinks::to_index(VectorI r) const {
    auto verify_in_range = [this](std::size_t i) {
        if (i >= m_link_pointers.size()) {
            throw Error("MapLinks::to_index: failed to map vector to index correctly.");
        }
        return i;
    };

    return verify_in_range([r, this]() {

    static constexpr const std::size_t k_bottom_left  = 0;
    static constexpr const std::size_t k_bottom_right = 1;
    static constexpr const std::size_t k_top_left     = 2;
    static constexpr const std::size_t k_top_right    = 3;

    static constexpr const std::size_t k_left_off = 4;
    const std::size_t k_right_off  = k_left_off  + m_height;
    const std::size_t k_top_off    = k_right_off + m_height;
    const std::size_t k_bottom_off = k_top_off   + m_width ;

    if (r.x == -1) {
        if (r.y > -1 && r.y < m_height) return k_left_off + (m_height - r.y);
        if (r.y ==       -1) return k_top_left;
        if (r.y == m_height) return k_bottom_left;
    } else if (r.x == m_width) {
        if (r.y > -1 && r.y < m_height) return k_right_off + (m_height - r.y);
        if (r.y ==       -1) return k_top_right;
        if (r.y == m_height) return k_bottom_right;
    } else if (r.y == -1) {
        assert(r.x != -1 && r.x != m_width);
        if (r.x > -1 && r.x < m_width) return k_top_off + (m_width - r.x);
    } else if (r.y == m_height) {
        assert(r.x != -1 && r.x != m_width);
        if (r.x > -1 && r.x < m_width) return k_bottom_off + (m_width - r.x);
    }
    throw InvArg("MapLinks::to_index: given vector is not an edge vector.");

    }());
}
