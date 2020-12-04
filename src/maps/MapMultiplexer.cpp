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

#include "MapMultiplexer.hpp"
#include "Maps.hpp"
#include "MapObjectLoader.hpp"

#include <tmap/TiledMap.hpp>

#include <cassert>

namespace {

using ElementPtr = MapMultiplexer::ElementPtr;
using StringCIter = std::string::const_iterator;

constexpr const int k_unset_depth = std::numeric_limits<int>::max();

/// @note does not load linked maps
void prepare_element(/*const std::string & filename, MultiMapLoader &,*/
                     MapMultiplexerElement &/*, VectorD translation = VectorD()*/);

void prepare_element_links(MapMultiplexerElement &, MultiMapLoader &,
                           std::vector<ElementPtr> &, VectorD translation = VectorD(),
                           int max_depth = k_unset_depth);

} // end of <anonymous> namespace

MultiMapLoader::MultiMapLoader() {
    m_maps.set_loader_function(TypeTag<MapInfo>(),
        [](const std::string & filename, MapInfo & info)
    {
        auto tmap_ptr = std::make_unique<tmap::TiledMap>();
        tmap_ptr->load_from_file(filename);

        auto lmap_ptr = std::make_unique<LineMap>();
        lmap_ptr->load_map_from(*tmap_ptr);

        info.linemap  = std::move(lmap_ptr);
        info.tiledmap = std::move(tmap_ptr);
    });
}

void MapMultiplexer::load_start
    (const std::string & filename)
{
    m_other_regions.clear();

    m_subject = std::make_shared<MapMultiplexerElement>();
#   if 1
    m_subject->set_source(m_loader.load_map(filename), VectorD());
    prepare_element(*m_subject);
#   endif
#   if 0
    prepare_element(filename, m_loader, *m_subject);
#   endif
    prepare_element_links(*m_subject, m_loader, m_other_regions);
    m_other_regions.push_back(m_subject);
}

namespace {

struct LinkInfo {
    std::string filename;
    VectorI offset;
    MapEdge edge;
};

inline bool is_semicolon(char c) { return c == ';'; }

LinkInfo load_link(const std::string & link_string);

[[deprecated]] VectorD get_translation_from_edge(const LineMap &, VectorI offset, MapEdge edge);

void prepare_element
    (/*const std::string & filename, MultiMapLoader & loader,*/
     MapMultiplexerElement & mme/*, VectorD translation*/)
{
#   if 0
    mme.set_source(loader.load_map(filename), translation);
#   endif
    mme.links.set_dimensions(mme.width(), mme.height());

    const auto & map_props = mme.get_tmap().map_properties();
    auto itr = map_props.find("map-link-depth");
    if (itr != map_props.end()) {
        if (!string_to_number(itr->second, mme.max_link_depth)) {
            // ideal emit a warning
        }
    }

    itr = map_props.find("map-persists");
    if (itr != map_props.end()) {
        if (itr->second == "true") {
            mme.persists = true;
        } else if (itr->second == "false") {
            mme.persists = false;
        } else {
            // emit a warning
        }
    }
}

void prepare_element_links
    (MapMultiplexerElement & mme, MultiMapLoader & loader,
     std::vector<ElementPtr> & vec, VectorD translation, int max_depth)
{
    if (max_depth == k_unset_depth) {
        max_depth = mme.max_link_depth;
    }
    if (max_depth < 1) return;

    const auto & map_props = mme.get_tmap().map_properties();
    auto itr = map_props.find("map-link-names");
    if (itr == map_props.end()) return;

    for_split<is_semicolon>(itr->second, [&](StringCIter beg, StringCIter end) {
        trim<is_whitespace>(beg, end);
        auto jtr = map_props.find(std::string { beg, end });
        if (jtr == map_props.end()) {
            // emit warning
            return;
        }

        LinkInfo nfo = load_link(jtr->second);
        if (nfo.filename.empty()) return;

        auto ptr = std::make_shared<MapMultiplexerElement>();
        vec.push_back(ptr);

        VectorD translation_ = translation + get_translation_from_edge(mme.map, nfo.offset, nfo.edge);
        ptr->set_source(loader.load_map(nfo.filename), translation_);
        prepare_element(*ptr);
        prepare_element_links(*ptr, loader, vec, translation_, max_depth - 1);
    });
}

// ----------------------------------------------------------------------------

MapEdge to_map_edge(const char * beg, const char * end);

VectorI read_offset(const char * beg, const char * end, MapEdge);

LinkInfo load_link(const std::string & link_string) {
    LinkInfo rv;
    enum { k_read_edge, k_read_offset, k_read_filename, k_warned, k_done };
    auto phase = k_read_edge;
    for_split<is_semicolon>(link_string.c_str(), link_string.c_str() + link_string.length(),
        [&rv, &phase](const char * beg, const char * end)
    {
        trim<is_whitespace>(beg, end);
        switch (phase) {
        case k_read_edge:
            switch (rv.edge = to_map_edge(beg, end)) {
            case MapEdge::bottom_right: case MapEdge::bottom_left:
            case MapEdge::top_right   : case MapEdge::top_left   :
                phase = k_read_filename;
                return;
            case MapEdge::right: case MapEdge::left  :
            case MapEdge::top  : case MapEdge::bottom:
                phase = k_read_offset;
                return;
            case MapEdge::not_an_edge:
                // emit warning
                phase = k_warned;
                return;
            default: throw BadBranchException();
            }
            return;
        case k_read_offset:
            rv.offset = read_offset(beg, end, rv.edge);
            phase = k_read_offset;
            return;
        case k_read_filename:
            rv.filename = std::string(beg, end);
            phase = k_done;
            return;
        case k_warned: return;
        default:
            // emit warning
            return;
        }
    });
    return rv;
}

VectorD get_translation_from_edge(const LineMap &, VectorI offset, MapEdge edge) {
    return VectorD();
}

// ----------------------------------------------------------------------------

MapEdge to_map_edge(const char * beg, const char * end) {
    auto eq = [beg, end](const char * kstr)
        { return std::equal(beg, end, kstr); };
    if (eq("left"  )) return MapEdge::left ;
    if (eq("right" )) return MapEdge::right ;
    if (eq("bottom")) return MapEdge::bottom;
    if (eq("top"   )) return MapEdge::top   ;
    if (eq("top-left"    ) || eq("left-top"    )) return MapEdge::top_left    ;
    if (eq("top-right"   ) || eq("right-top"   )) return MapEdge::top_right   ;
    if (eq("bottom-left" ) || eq("left-bottom" )) return MapEdge::bottom_left ;
    if (eq("bottom-right") || eq("right-bottom")) return MapEdge::bottom_right;
    return MapEdge::not_an_edge;
}

VectorI read_offset(const char * beg, const char * end, MapEdge edge) {
    switch (edge) {
    case MapEdge::bottom_right: case MapEdge::bottom_left:
    case MapEdge::top_right   : case MapEdge::top_left   :
    case MapEdge::not_an_edge :

        throw std::invalid_argument("read_offset: this map edge type cannot have an offset");
    case MapEdge::right: case MapEdge::left  :
    case MapEdge::top  : case MapEdge::bottom:



    default: throw BadBranchException();
    }
}

} // end of <anonymous> namespace
