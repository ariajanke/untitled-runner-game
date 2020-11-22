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
#include "../Components.hpp"

#include <map>
#include <string>
#include <memory>

class ScaleLoader {
public:
    void register_scale_left_part (const std::string &, Entity);
    void register_scale_right_part(const std::string &, Entity);
    void register_scale_pivot_part(const std::string &, Entity);
private:
    struct ScaleRecord {
        Entity pivot, left, right;
    };
    void check_complete_scale_record(const ScaleRecord &);
    std::map<std::string, ScaleRecord> m_scale_records;
};

using WaypointsTag = TypeTag<Platform>;

class MapObjectLoader :
    public CachedLoader<SpriteSheet>,
    public CachedLoader<std::vector<VectorD>, WaypointsTag>,
    public ScaleLoader
{
public:
    using CachedLoader<SpriteSheet>::load;
    using CachedLoader<std::vector<VectorD>, WaypointsTag>::load;

    using WaypointsPtr = Platform::WaypointsPtr;
    MapObjectLoader();

    virtual ~MapObjectLoader();
    virtual Entity create_entity() = 0;
    virtual void set_player(Entity) = 0;
#   if 0
    virtual WaypointsPtr find_waypoints(const std::string &) = 0;
    virtual void load_waypoints(const std::string &, std::vector<VectorD> && waypoints) = 0;
#   endif
    virtual std::default_random_engine & get_rng() = 0;
};

namespace tmap { struct MapObject; }
using MapObjectLoaderFunction = void(*)(MapObjectLoader &, const tmap::MapObject &);

MapObjectLoaderFunction get_loader_function(const std::string &);

