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

class RecallBoundsLoader {
public:
    RecallBoundsLoader() {}
    RecallBoundsLoader(const RecallBoundsLoader &) = default;
    RecallBoundsLoader(RecallBoundsLoader &&) = default;

    // does nothing if there's an active exception
    ~RecallBoundsLoader();

    RecallBoundsLoader & operator = (const RecallBoundsLoader &) = default;
    RecallBoundsLoader & operator = (RecallBoundsLoader &&) = default;

    void register_recallable(const std::string & bounds_entity_name, Entity);
    void register_bounds    (const std::string & bounds_entity_name, const Rect &);

private:
    struct RecallRecord {
        std::vector<Entity> recallables;
        Rect bounds = ReturnPoint().recall_bounds;
    };

    std::map<std::string, RecallRecord> m_records;
};

using WaypointsTag = TypeTag<Platform>;

class CachedItemAnimations {
public:
    void load_animation(Item & item, const tmap::MapObject & obj) {
        auto & ptr = m_map[obj.tile_set->convert_to_gid(obj.local_tile_id)];
        if (!ptr.expired()) {
            item.collection_animation = ptr.lock();
            return;
        }

        const auto * props = obj.tile_set->properties_on(obj.local_tile_id);
        if (!props) return;
        auto itr = props->find("on-collection");
        if (itr == props->end()) return;

        const char * beg = &itr->second.front();
        const char * end = beg + itr->second.size();

        ItemCollectionAnimation ica;
        ica.tileset = obj.tile_set;
        int num = 0;
        for_split<is_colon>(beg, end, [&ica, &num](const char * beg, const char * end) {
            if (num == 0) {
                trim<is_whitespace>(beg, end);
                if (!string_to_number(beg, end, ica.time_per_frame)) {}
            } else if (num == 1) {
                auto & vec = ica.tile_ids;
                for_split<is_comma>(beg, end, [&vec](const char * beg, const char * end) {
                    int i = 0;
                    trim<is_whitespace>(beg, end);
                    if (string_to_number(beg, end, i)) {
                        vec.push_back(i);
                    } else {}
                });
            }
            ++num;
        });
        ptr = (item.collection_animation = std::make_shared<ItemCollectionAnimation>(std::move(ica)));
    }
private:
    static bool is_colon(char c) { return c == ':'; }
    static bool is_comma(char c) { return c == ','; }
    std::map<int, std::weak_ptr<ItemCollectionAnimation>> m_map;
};

class MapObjectLoader :
    public CachedLoader<SpriteSheet, std::string>,
    public CachedLoader<std::vector<VectorD>, std::string, WaypointsTag>,
    //public CachedLoader<ItemCollectionAnimation>,
    public CachedItemAnimations,
    public ScaleLoader, public RecallBoundsLoader
{
public:
    using CachedLoader<SpriteSheet, std::string>::load;
    using CachedLoader<std::vector<VectorD>, std::string, WaypointsTag>::load;

    using WaypointsPtr = Platform::WaypointsPtr;
    MapObjectLoader();

    virtual ~MapObjectLoader();
    virtual Entity create_entity() = 0;
    virtual void set_player(Entity) = 0;
    virtual std::default_random_engine & get_rng() = 0;
};

namespace tmap { struct MapObject; }
using MapObjectLoaderFunction = void(*)(MapObjectLoader &, const tmap::MapObject &);

MapObjectLoaderFunction get_loader_function(const std::string &);

