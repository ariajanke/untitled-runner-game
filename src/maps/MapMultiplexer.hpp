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

#include "SurfaceRef.hpp"
#include "MapLinks.hpp"
#include "Maps.hpp"

class MapObjectLoader;
class LineMap;

class MapMultiplexerLayer final {
public:
    Surface operator () (int x, int y, int segnum) const;
    SurfaceRef ref(int x, int y, int segnum) const;
};

namespace tmap { class TiledMap; }

class MultiMapLoader {
public:
    struct MapInfo {
        std::unique_ptr<const tmap::TiledMap> tiledmap;
        std::unique_ptr<const LineMap       > linemap;
    };

    MultiMapLoader();

    std::shared_ptr<MapInfo> load_map(const std::string & filename)
        { return m_maps.load(TypeTag<MapInfo>(), filename); }
private:
    CachedLoader<MapInfo, std::string> m_maps;
};

class MapMultiplexerElement final {
public:
    using LoaderInfo = MultiMapLoader::MapInfo;

    MapLinks links;
    std::vector<std::weak_ptr<MapMultiplexerElement>> linked_maps;

    int max_link_depth = 0;
    bool persists = false;

    LineMap map;

    void make_blank_of_size(int width, int height)
        { map.make_blank_of_size(width, height); }

    int width () const { return map.width (); }
    int height() const { return map.height(); }

    void set_source(std::shared_ptr<LoaderInfo> nfo, VectorD translation) {
        m_cshrptr = nfo;
        map       = *nfo->linemap;

        map.set_translation(translation);
    }

    const tmap::TiledMap & get_tmap() const { return *m_cshrptr->tiledmap; }

private:
    std::shared_ptr<LoaderInfo> m_cshrptr;
};

bool is_on_edge(VectorD, const LineMap &, Layer);
bool is_inside(VectorD, const LineMap &);

class MapMultiplexer final {
public:
    using ElementPtr = std::shared_ptr<MapMultiplexerElement>;

    void set_subject_location(MapObjectLoader &, VectorD location, Layer layer) {
        // there are three posibilities:
        // - on subject map
        // - on subject map edge
        // - outside of subject map (we really want teleportation to be possible)
        if (is_inside(location, m_subject->map)) {
            return;
        } else if (is_on_edge(location, m_subject->map, layer)) {
            // moving to linked map
            ;
        } else {
            // teleportation
            // we'll have to check all loaded maps
            ;
        }
    }
    MapMultiplexerLayer & get_layer(Layer);
    void load_start(const std::string & filename);
    bool location_is_on_map(VectorD) const;
private:
    static constexpr const double k_uninit_tile_size = 0.;

    MultiMapLoader m_loader;

    std::vector<ElementPtr> m_other_regions;
    ElementPtr m_subject;
    // limitation: all maps must have the same tile size
    double m_tile_width  = k_uninit_tile_size;
    double m_tile_height = k_uninit_tile_size;
};
