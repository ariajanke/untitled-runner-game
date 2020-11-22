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

#include "LineMapLoader.hpp"
#include "../GridRange.hpp"

#include <tmap/TiledMap.hpp>

#include <cassert>

namespace {

using Error             = std::runtime_error;
using InvArg            = std::invalid_argument;
using SegmentsPtr       = LineMapLoader::SegmentsPtr;
using LineViewMap       = LineMapLoader::LineViewMap;
using TilePropertyMap   = tmap::TilePropertiesInterface::PropertyMap;
using TileInfo          = LineMapLoader::TileInfo;
using SegmentMap        = LineMapLoader::SegmentMap;
using SurfaceDetailsPtr = LineMapLoader::SurfaceDetailsPtr;
using SurfaceDetailsMap = LineMapLoader::SurfaceDetailsMap;
using SegmentsInfo      = LineMapLoader::SegmentsInfo;

/// value known from TilEd as the universally "empty" tile
constexpr const int k_empty_tile_gid = 0;

/// @returns tile gids, if the tile has no segments, it's gid value is replaced
/// with k_empty_tile_gid
Grid<int> get_layer_gids(const tmap::TiledMap &, int width, int height, const SegmentMap &, const char * layer_name);
Grid<int> get_layer_gids(const tmap::TiledMap &, const SegmentMap &, const char * layer_name);

std::pair<SegmentsPtr, LineViewMap> produce_segment_view_map(const SegmentsInfo &);
std::pair<SurfaceDetailsPtr, SurfaceDetailsMap>
    produce_surface_details_map(const SegmentsInfo &);

void overwrite_layer(Grid<int> &, const Grid<int> &, const char * layer_name);

TileInfo load_tile_info(const TilePropertyMap &);

template <typename T>
T mv_copy(const T & obj) {
    T rv(obj);
    return rv;
}

template <typename Type>
GridRange<Type> compute_range_for_tiles
    (Grid<Type> &, VectorD a, VectorD b, double tile_width, double tile_height);

} // end of <anonymous> namespace

void LineMapLoader::load_map(const tmap::TiledMap & map) {
    load_tile_size(map);

    auto nfo = load_tileset_map(map);

    auto groundgids = get_layer_gids(map, nfo.segment_map, k_ground);
    int width  = groundgids.width ();
    int height = groundgids.height();
    auto foregids = get_layer_gids(map, width, height, nfo.segment_map, k_foreground);
    auto backgids = get_layer_gids(map, width, height, nfo.segment_map, k_background);

    overwrite_layer(foregids, groundgids, k_foreground);
    overwrite_layer(backgids, groundgids, k_background);

    auto [segs     , segs_map    ] = produce_segment_view_map   (nfo);
    auto [surf_dets, surf_det_map] = produce_surface_details_map(nfo);

    m_segments = segs;
    m_details  = surf_dets;

    auto layers = {
        std::make_tuple(foregids, std::ref(m_foreground), std::ref(m_foreground_details)),
        std::make_tuple(backgids, std::ref(m_background), std::ref(m_background_details))
    };
    for (auto & [grid, seggrid, detgrid] : layers) {
        seggrid.set_size(width, height);
        detgrid.set_size(width, height, mv_copy(nullptr));
        assert(grid.width() == seggrid.width() && grid.height() == seggrid.height());
        assert(detgrid.width() == seggrid.width() && detgrid.height() == seggrid.height());
        for (VectorI r; r != grid.end_position(); r = grid.next(r)) {
            if (grid(r) == k_empty_tile_gid) continue;
            auto segitr = segs_map    .find(grid(r));
            auto detitr = surf_det_map.find(grid(r));
            if (segitr == segs_map.end() || detitr == surf_det_map.end()) {
                throw std::runtime_error("LineMapLoader::load_map: non empty tile has missing info");
            }
            seggrid(r) = segitr->second;
            detgrid(r) = detitr->second;
        }
    }

    load_transition_tiles(map, m_transition_tiles);
}

void LineMapLoader::load_layer_into
    (Grid<LinesView> & grid, Grid<const SurfaceDetails *> & dets, Layer layer)
{
    Grid<LinesView>    * src_grid = nullptr;
    SurfaceDetailsGrid * src_dets = nullptr;
    switch (layer) {
    case Layer::neither:
        throw InvArg("LineMapLoader::load_layer_into: layer maybe foreground "
                     "or background only.");
    case Layer::background:
        src_grid = &m_background;
        src_dets = &m_background_details;
        break;
    case Layer::foreground:
        src_grid = &m_foreground;
        src_dets = &m_foreground_details;
        break;
    }
    grid.clear();
    grid.swap(*src_grid);

    dets.clear();
    dets.swap(*src_dets);
}

void LineMapLoader::load_transitions_into(Grid<bool> & grid) {
    grid.clear();
    grid.swap(m_transition_tiles);
}

/* private */ void LineMapLoader::load_tile_size(const tmap::TiledMap & map) {
    for (auto layername : k_layer_list) {
        auto * layer = map.find_tile_layer(layername);
        if (!layer) {
            if (layername == std::string(k_ground)) {
                throw Error("LineMapLoader::load_map: missing required "
                            "\"ground\" layer.");
            }
            continue;
        }
        using EqualToD = std::equal_to<double>;
        if (EqualToD()(m_tile_width , k_initial_tile_size) &&
            EqualToD()(m_tile_height, k_initial_tile_size))
        {
            m_tile_width  = double(layer->tile_width());
            m_tile_height = double(layer->tile_height());
        }
        if (!EqualToD()(m_tile_width , double(layer->tile_width())) ||
            !EqualToD()(m_tile_height, double(layer->tile_height())))
        {
            throw Error("LineMapLoader::load_map: all layers must have tiles "
                        "of the same size.");
        }
    }
}

/* private */ LineMapLoader::SegmentsInfo LineMapLoader::load_tileset_map
    (const tmap::TiledMap & map) const
{
    assert(has_tile_size_initialized());

    std::unordered_map<int, TileInfo> segment_map;
    int total_segments_count = 0;
    for (auto layername : k_layer_list) {
        auto * layer = map.find_tile_layer(layername);
        // we've established that there is definitely a ground layer
        if (!layer) { continue; }
        // better: be able to iterate tilesets themselves for this information
        for (int y = 0; y != layer->height(); ++y) {
        for (int x = 0; x != layer->width (); ++x) {
            auto * properties = (*layer)(x, y);
            if (!properties) continue;
            auto itr = segment_map.find(layer->tile_gid(x, y));
            if (itr != segment_map.end()) continue;
            // great, we get to load up info if it's not filled!
            auto tileinfo = load_tile_info(*properties);
            for (auto & line : tileinfo.segments) {
                line.a.x *= tile_width ();
                line.a.y *= tile_height();
                line.b.x *= tile_width ();
                line.b.y *= tile_height();
            }

            if (tileinfo.segments.empty()) continue;
            total_segments_count += int(tileinfo.segments.size());
            segment_map[layer->tile_gid(x, y)] = std::move(tileinfo);
        }}
    }
    return SegmentsInfo { segment_map, total_segments_count };
}

/* private */ void LineMapLoader::load_transition_tiles
    (const tmap::TiledMap & map, Grid<bool> & grid) const
{
    assert(has_tile_size_initialized());

    grid.set_size(m_foreground.width(), m_foreground.height(), false);
    for (const auto & obj : map.map_objects()) {
        if (obj.type != k_transition_object) continue;
        VectorD a(sf::Vector2f(obj.bounds.left, obj.bounds.top));
        VectorD b = a + VectorD(sf::Vector2f(obj.bounds.width, obj.bounds.height));
        auto range = compute_range_for_tiles(grid, a, b, tile_width(), tile_height());
        using RefType = Grid<bool>::ReferenceType;
        for (RefType b : range) { b = true; }
    }
}

namespace {

inline bool is_lines_delim(char c) { return c == ';'; }
inline bool is_point_delim(char c) { return c == ':'; }
inline bool is_tuple_delim(char c) { return c == ','; }

template <typename Type>
VectorI limit_vector_to(const Grid<Type> &, VectorI);

template <typename Key>
const std::string * find_property(const TilePropertyMap &, const Key &);

TileInfo load_tile_info(const TilePropertyMap & pmap) {
    auto * lines = find_property(pmap, "lines");
    if (!lines) return TileInfo();

    TileInfo rv;
    if (auto * hard_ceil = find_property(pmap, "hard-ceiling")) {
        rv.hard_ceilling = *hard_ceil == "true";
    }

    static const constexpr auto k_exactly_points_msg =
        "load_line_pairs: Each point pair must have exactly two points.";
    static const constexpr auto k_exactly_two_nums_msg =
        "load_line_pairs: Each point must have exactly two numbers.";
    static const constexpr auto k_failed_str_to_num_msg =
        "load_line_pairs: Cannot convert string to number for vector pairs.";

    auto & segments = rv.segments;
    for_split<is_lines_delim>(lines->c_str(), lines->c_str() + lines->length(),
        [&segments](const char * beg, const char * end)
    {
        VectorD a, b;
        auto pts = { &a, &b };
        auto vitr = pts.begin();
        ::for_split<is_point_delim>(beg, end,
            [&pts, &vitr](const char * beg, const char * end)
        {
            if (vitr == pts.end()) throw Error(k_exactly_points_msg);
            auto nums = { &(**vitr).x, &(**vitr).y };
            ++vitr;
            auto itr = nums.begin();
            ::for_split<is_tuple_delim>(beg, end,
                [&itr, &nums](const char * beg, const char * end)
            {
                if (itr == nums.end())
                    throw Error(k_exactly_two_nums_msg);
                trim<is_whitespace>(beg, end);
                if (!string_to_number(beg, end, **(itr++)))
                    throw Error(k_failed_str_to_num_msg);
            });
            if (itr != nums.end()) throw Error(k_exactly_two_nums_msg);
        });
        if (vitr != pts.end()) throw Error(k_exactly_points_msg);
        segments.emplace_back(a, b);
    });
    return rv;
}

Grid<int> get_layer_gids(const tmap::TiledMap & map, const SegmentMap & surfacemap, const char * layer_name) {
    auto layer = map.find_tile_layer(layer_name);
    if (!layer) {
        throw Error("Could not find required layer \"" + std::string(layer_name) + "\"");
    }
    return get_layer_gids(map, layer->width(), layer->height(), surfacemap, layer_name);
}

Grid<int> get_layer_gids(const tmap::TiledMap & map, int width, int height, const SegmentMap & surfacemap, const char * layer_name) {
    Grid<int> rv;
    rv.set_size(width, height, mv_copy(k_empty_tile_gid));
    auto layer = map.find_tile_layer(layer_name);
    if (!layer) return rv;
    for (VectorI r; r != rv.end_position(); r = rv.next(r)) {
        auto gid = layer->tile_gid(r.x, r.y);
        auto itr = surfacemap.find(gid);
        if (itr == surfacemap.end()) continue;
        rv(r) = gid;
    }
    return rv;
}

std::pair<SegmentsPtr, LineViewMap>
    produce_segment_view_map(const SegmentsInfo & nfo)
{
    // prepare line segments container
    // prevent iterator invalidation
    SegmentsPtr segments = std::make_shared<std::vector<LineSegment>>();
    segments->reserve(std::size_t(nfo.total_segments_count));
    LineViewMap view_map;
    for (auto & pair : nfo.segment_map) {
        auto f = segments->end();
        auto offset = segments->size();
        auto & source_segments = pair.second.segments;
        segments->insert(segments->end(), source_segments.begin(), source_segments.end());
        assert(f == (segments->begin() + int(offset)));
        view_map[pair.first] = LinesView(f, segments->end());
    }
    return std::make_pair(segments, view_map);
}

std::pair<SurfaceDetailsPtr, SurfaceDetailsMap>
    produce_surface_details_map(const SegmentsInfo & nfo)
{
    static const SurfaceDetails k_default_details;
    auto details = std::make_shared<SurfaceDetailsPtr::element_type>();

    std::size_t diff_count = 0;
    for (const auto & [gid, tilenfo] : nfo.segment_map) {
        (void)gid;
        auto dets = static_cast<const SurfaceDetails &>(tilenfo);
        if (dets != k_default_details) { diff_count++; }
    }
    details->reserve(diff_count);

    SurfaceDetailsMap viewmap;
    for (const auto & [gid, tilenfo] : nfo.segment_map) {
        auto dets = static_cast<const SurfaceDetails &>(tilenfo);
        if (dets == k_default_details) {
            viewmap[gid] = &k_default_details;
        } else {
            [[maybe_unused]] auto old_cap = details->capacity();
            details->push_back(dets);
            // *must* not reallocate!
            assert(old_cap == details->capacity());
            viewmap[gid] = &details->back();
        }
    }
    return std::make_pair(details, viewmap);
}

void overwrite_layer
    (Grid<int> & layer,
     const Grid<int> & overwriting_layer, const char * layer_name)
{
    assert(layer.width () == overwriting_layer.width () &&
           layer.height() == overwriting_layer.height() );
    std::vector<VectorI> mismatch_tiles;
    for (VectorI r; r != layer.end_position(); r = layer.next(r)) {
        if (layer(r) == k_empty_tile_gid) {
            layer(r) = overwriting_layer(r);
        } else if (overwriting_layer(r) == k_empty_tile_gid) {
            // do nothing
        } else if (layer(r) != overwriting_layer(r)) {
            mismatch_tiles.push_back(r);
        }
    }
    if (!mismatch_tiles.empty()) {
        std::string tilestr;
        for (auto r : mismatch_tiles) {
            tilestr += "(" + std::to_string(r.x) + ", " + std::to_string(r.y) + ") ";
        }
        tilestr.pop_back();
        throw Error("overwrite_layer: tile mismatch between ground and " +
                    std::string(layer_name) + " layer at the following locations: " +
                    tilestr);
    }
}

template <typename Type>
GridRange<Type> compute_range_for_tiles
    (Grid<Type> & grid, VectorD a, VectorD b,
     double tile_width, double tile_height)
{
    VectorD minv(std::min(a.x, b.x),
                 std::min(a.y, b.y));
    VectorD maxv(std::max(a.x, b.x),
                 std::max(a.y, b.y));
    VectorI minvi(int(minv.x / tile_width), int(minv.y / tile_height));
    VectorI maxvi(int(maxv.x / tile_width), int(maxv.y / tile_height));

    minvi = limit_vector_to(grid, minvi);
    maxvi = limit_vector_to(grid, maxvi) + VectorI(1, 1);

    return GridRange<Type>(grid, minvi.x, minvi.y, maxvi.x, maxvi.y);
}

// ----------------------------------------------------------------------------

template <typename Type>
VectorI limit_vector_to(const Grid<Type> & grid, VectorI r) {
    return VectorI(std::max(std::min(r.x, grid.width () - 1), 0),
                   std::max(std::min(r.y, grid.height() - 1), 0));
}

template <typename Key>
const std::string * find_property(const TilePropertyMap & map, const Key & key) {
    auto itr = map.find(key);
    if (itr == map.end()) return nullptr;
    return &itr->second;
}

} // end of <anonymous> namespace
