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
    void load_animation(TriggerBox &, const tmap::MapObject &);

private:
    static bool is_colon(char c) { return c == ':'; }
    static bool is_comma(char c) { return c == ','; }
    std::shared_ptr<const ItemCollectionInfo> load(const tmap::MapObject &);

    std::map<int, std::weak_ptr<ItemCollectionInfo>> m_map;
};

class CachedWaypoints {
public:
    using WaypointsPtr = Waypoints::ContainerPtr;
    using WaypointsContainer = Waypoints::Container;
    WaypointsPtr load_waypoints(const tmap::MapObject * obj);

private:
    WaypointsPtr m_no_obj_waypoints;
    std::map<std::string, std::weak_ptr<WaypointsContainer>> m_waypt_map;
};

class MapObjectLoader :
    public CachedLoader<SpriteSheet, std::string>,
    public CachedWaypoints, public CachedItemAnimations,
    public ScaleLoader, public RecallBoundsLoader
{
public:
    using CachedLoader<SpriteSheet, std::string>::load;
    using WaypointsPtr = Waypoints::ContainerPtr;

    MapObjectLoader();

    virtual ~MapObjectLoader();
    virtual Entity create_entity() = 0;
    virtual void set_player(Entity) = 0;
    virtual std::default_random_engine & get_rng() = 0;
    virtual const tmap::MapObject * find_map_object(const std::string &) const = 0;
};

namespace tmap { struct MapObject; }
using MapObjectLoaderFunction = void(*)(MapObjectLoader &, const tmap::MapObject &);

MapObjectLoaderFunction get_loader_function(const std::string &);

// -------------------------------- utils -------------------------------------

template <typename IntType, typename Func>
class OptionalRequiresNumeric {
public:
    using IntegerType = IntType;
    explicit OptionalRequiresNumeric(Func && f_): f(std::move(f_)) {}
    std::runtime_error operator ()
        (const std::string & key, const std::string & val) const noexcept
    { return f(key, val); }

private:
    Func f;
};

template <typename IntType, typename Func>
auto make_optional_requires_numeric(Func && f) {
    return OptionalRequiresNumeric<IntType, Func>(std::move(f));
}

template <typename Key, typename T, typename Compare>
class MapValueFinder {
public:
    using MapType = std::map<Key, T, Compare>;

    explicit MapValueFinder(const MapType & map_): map(map_) {}

    template <typename Key2, typename Func>
    void operator () (Key2 saught_key, Func && f) const {
        auto itr = map.find(saught_key);
        if (itr == map.end()) return;
        const auto & obj = itr->second;
        (void)f(obj);
    }

    template <typename Key2, typename Func1, typename IntType, typename Func2>
    void operator () (const Key2 & key,
                      const OptionalRequiresNumeric<IntType, Func1> & throw_f,
                      Func2 && do_f) const
    {
        auto itr = map.find(key);
        if (itr == map.end()) return;

        auto vbeg = itr->second.begin();
        auto vend = itr->second.end  ();
        trim<is_whitespace>(vbeg, vend);

        IntType datum = 0;
        if (!string_to_number_multibase(vbeg, vend, datum)) {
            throw throw_f(itr->first, itr->second);
        }
        do_f(datum);
    }
private:
    const MapType & map;
};

template <typename Key, typename T, typename Compare>
MapValueFinder<Key, T, Compare> make_do_if_found(const std::map<Key, T, Compare> & map) {
    return MapValueFinder(map);
}

template <typename T, typename U>
std::vector<T> convert_vector_to(const std::vector<U> & vec) {
    static_assert(std::is_constructible_v<U, T>, "Cannot convert source vector.");
    std::vector<T> rv;
    rv.reserve(vec.size());
    for (const auto & src : vec)
        rv.emplace_back(src);
    return rv;
}

template <typename T, typename Func>
void for_side_by_side(const std::vector<T> & vec, Func && f) {
    if (vec.empty()) return;
    auto itr = vec.begin();
    auto jtr = itr + 1;
    for (; jtr != vec.end(); ++itr, ++jtr) {
        const auto & lhs = *itr;
        const auto & rhs = *jtr;
        if (adapt_to_flow_control_signal(std::move(f), lhs, rhs) == fc_signal::k_break)
            return;
    }
}

VectorD parse_vector(std::string::const_iterator beg, std::string::const_iterator end);

VectorD parse_vector(const std::string &);
