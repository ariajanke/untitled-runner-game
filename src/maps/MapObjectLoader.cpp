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

#include "MapObjectLoader.hpp"
#include "Maps.hpp"

#include <tmap/TiledMap.hpp>
#include <tmap/TileSet.hpp>

#include <common/StringUtil.hpp>

#include <iostream>
#include <set>

#include <cstring>
#include <cassert>

namespace {

using MapObject = tmap::MapObject;

class ObjectTypeLoader : public ObjectLoader {
public:
    // should be statically initialized :)
    virtual const std::vector<std::string> & get_requirement_names() const noexcept = 0;
};

//using TypeLoaderNameCallback = ObjectTypeLoader::DependCheckCallback;

template <void (*kt_loader_func)(MapObjectLoader &, const MapObject &)>
std::unique_ptr<ObjectTypeLoader> make_type_loader() {
    class TypeLoaderComplete final : public ObjectTypeLoader {
        const std::vector<std::string> & get_requirement_names() const noexcept override {
            static std::vector<std::string> inst;
            return inst;
        }
        void operator () (MapObjectLoader & loader, const MapObject & obj) const override
            { kt_loader_func(loader, obj); }
    };
    return std::make_unique<TypeLoaderComplete>();
}

template <void (*kt_loader_func)(MapObjectLoader &, const MapObject &),
          const std::vector<std::string> & (*kt_get_requirement_func)()>
std::unique_ptr<ObjectTypeLoader> make_type_loader() {
    class TypeLoaderComplete final : public ObjectTypeLoader {
        const std::vector<std::string> & get_requirement_names() const noexcept override
            { return kt_get_requirement_func(); }
        void operator () (MapObjectLoader & loader, const MapObject & obj) const override
            { kt_loader_func(loader, obj); }
    };
    return std::make_unique<TypeLoaderComplete>();
}

// type -> type loader
using MapLoadersMap = std::map<std::string, std::unique_ptr<ObjectTypeLoader>>;

void load_player_start (MapObjectLoader &, const MapObject &);
void load_snake        (MapObjectLoader &, const MapObject &);
void load_coin         (MapObjectLoader &, const MapObject &);
void load_diamond      (MapObjectLoader &, const MapObject &);
void load_launcher     (MapObjectLoader &, const MapObject &);
const std::vector<std::string> & get_launcher_requirements();

void load_platform     (MapObjectLoader &, const MapObject &);
void load_wall         (MapObjectLoader &, const MapObject &);
void load_ball         (MapObjectLoader &, const MapObject &);

void load_basket       (MapObjectLoader &, const MapObject &);
void load_checkpoint   (MapObjectLoader &, const MapObject &);

void load_scale_pivot (MapObjectLoader &, const MapObject &);
void load_scale_left  (MapObjectLoader &, const MapObject &);
void load_scale_right (MapObjectLoader &, const MapObject &);

const std::vector<std::string> & get_scale_part_requirements();

// some objects are just plain rectangles that just kinda exist
// some scripts/other entities may need to rely on them existing during
// gameplay and not just at map loading time
void load_rectangle(MapObjectLoader &, const MapObject &);

const auto k_reserved_objects = {
    k_line_map_transition_object
};

template <typename T>
LineSegment to_floor_segment(const sf::Rect<T> &);

}  // end of <anonymous> namespace

// ----------------------------------------------------------------------------

void CachedItemAnimations::load_animation
    (TriggerBox & tbox, const tmap::MapObject & obj)
{
    tbox.reset<ItemCollectionSharedPtr>() = load(obj);
}

std::shared_ptr<const ItemCollectionInfo> CachedItemAnimations::load(const tmap::MapObject & obj) {
    auto & ptr = m_map[obj.global_tile_id];
    if (!ptr.expired()) return ptr.lock();

    const auto * props = obj.tile_set->properties_of(obj.local_tile_id);
    if (!props) return nullptr;
    auto do_if_found = make_do_if_found(*props);
    ItemCollectionInfo ica;
    do_if_found("on-collection", [&obj, &ica](const std::string & val) {
        const char * beg = &val.front();
        const char * end = beg + val.size();

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
    });

    do_if_found("diamond-value", [&ica](const std::string & val) {
        auto beg = val.begin();
        auto end = val.end();
        trim<is_whitespace>(beg, end);
        if (!string_to_number(beg, end, ica.diamond_quantity)) {
            // warn or something
        }
    });

    auto rv = std::make_shared<ItemCollectionInfo>(ica);
    ptr = rv;
    return rv;
}

// ----------------------------------------------------------------------------

CachedWaypoints::WaypointsPtr CachedWaypoints::load_waypoints(const tmap::MapObject * obj) {
    if (!obj) {
        // log: no waypoints of name found
        if (!m_no_obj_waypoints) {
            m_no_obj_waypoints = std::make_shared<WaypointsContainer>();
        }
        return m_no_obj_waypoints;
    }

    auto & ptr = m_waypt_map[obj->name];
    if (!ptr.expired()) { return ptr.lock(); }

    WaypointsContainer waypoints = convert_vector_to<VectorD>(obj->points);
    auto sptr = std::make_shared<WaypointsContainer>();
    sptr->swap(waypoints);
    ptr = sptr;
    return sptr;
}

// ----------------------------------------------------------------------------

MapObjectLoader::MapObjectLoader() {
    CachedLoader<SpriteSheet, std::string>::set_loader_function(TypeTag<SpriteSheet>(), []
        (const std::string & filename, SpriteSheet & sptsheet)
    {
        sptsheet.load_from_file(filename);
    });
}

/* vtable anchor */ MapObjectLoader::~MapObjectLoader() {}

const auto k_loader_functions = [] () {
    MapLoadersMap loaders_map;
    auto add_once = [&loaders_map]
        (const std::string & typestr, std::unique_ptr<ObjectTypeLoader> && typeloader)
    {
        for (const auto & resname : k_reserved_objects) {
            assert(resname != typestr);
        }
        auto itr = loaders_map.find(typestr);
        assert(itr == loaders_map.end());
        loaders_map[typestr] = std::move(typeloader);
    };

    add_once("player-start"  , make_type_loader<load_player_start >());
    add_once("snake"         , make_type_loader<load_snake        >());
    add_once("coin"          , make_type_loader<load_coin         >());
    add_once("diamond"       , make_type_loader<load_diamond      >());
    add_once("launcher"      , make_type_loader<load_launcher     , get_launcher_requirements  >());
    add_once("platform"      , make_type_loader<load_platform     >());
    add_once("wall"          , make_type_loader<load_wall         >());
    add_once("ball"          , make_type_loader<load_ball         >());
    add_once("scale-left"    , make_type_loader<load_scale_left   , get_scale_part_requirements>());
    add_once("scale-right"   , make_type_loader<load_scale_right  , get_scale_part_requirements>());
    add_once("scale-pivot"   , make_type_loader<load_scale_pivot  >());
    add_once("basket"        , make_type_loader<load_basket       >());
    add_once("checkpoint"    , make_type_loader<load_checkpoint   >());
    add_once("rectangle"     , make_type_loader<load_rectangle>());
    add_once("balloon"       , make_type_loader<BalloonScript::load_balloon>());

    return loaders_map;
} ();

const ObjectTypeLoader * get_loader_function_(const std::string & type) {
    auto itr = k_loader_functions.find(type);
    if (itr == k_loader_functions.end()) return nullptr;
    return itr->second.get();
}

const ObjectLoader & get_loader_function(const std::string & type) {
    class NoTypeLoader final : public ObjectLoader {
        void operator () (MapObjectLoader &, const tmap::MapObject & obj) const override {
            // objects without types are ignored
            if (obj.type.empty()) return;
            std::cout << "[map load warning] Object of type \""
                      << obj.type << "\" is not handled by the map loader."
                      << std::endl;
        }
    };
    static NoTypeLoader s_no_type_instance;
    auto gv = get_loader_function_(type);
    return gv ? static_cast<const ObjectLoader &>(*gv) : s_no_type_instance;
}

static int count_dependent_depth
    (const tmap::MapObject & object,
     const std::map<const tmap::MapObject *, std::vector<const tmap::MapObject *>> & depmap,
     int depth = 0)
{
    // there's an optimization opportunity here
    // I'll leave it out unless there's some reason to believe it could be
    // useful
    auto itr = depmap.find(&object);
    if (itr == depmap.end()) {
        //assert(depth == 0);
        return depth;
    }
    const auto & deps = itr->second;
    if (deps.empty()) return depth;
    int rv = depth;
    for (const auto * depobj : deps) {
        rv = std::max(rv, count_dependent_depth(*depobj, depmap, depth + 1));
    }
    return rv;
}

static std::vector<const tmap::MapObject *> get_map_load_order_
    (const tmap::MapObject::MapObjectContainer & objects,
     std::map<std::string, const tmap::MapObject *> & namemap)
{
    namemap.clear();
    for (const auto & object : objects) {
        if (object.name.empty()) continue;
        auto success = namemap.insert(std::make_pair(object.name, &object)).second;
        if (!success) {
            throw std::runtime_error("Ambiguous object name \"" + object.name + "\" found on map.");
        }
    }

    std::map<const tmap::MapObject *, std::vector<const tmap::MapObject *>> depsmap;
    for (const auto & object : objects) {
        auto gv = get_loader_function_(object.type);
        if (!gv) continue;
        std::vector<const tmap::MapObject *> deps;
        deps.reserve(gv->get_requirement_names().size());
        for (const auto & depname : gv->get_requirement_names()) {
            auto prop_itr = object.custom_properties.find(depname);
            if (prop_itr == object.custom_properties.end()) {
                // if the object does not have the attribute, then the loader
                // function proper will have to determine whether it's optional
                // or not
                continue;
            }
            // same as note above
            if (prop_itr->second.empty()) { continue; }
            auto obj_itr = namemap.find(prop_itr->second);
            if (obj_itr == namemap.end()) {
                throw std::runtime_error("");
            }
            deps.push_back(obj_itr->second);
        }
        depsmap[&object] = std::move(deps);
    }

    using TempOrderTup = std::tuple<int, const tmap::MapObject *>;
    std::vector<TempOrderTup> order_temp;
    for (const auto & object : objects) {
        order_temp.emplace_back(count_dependent_depth(object, depsmap), &object);
    }
    std::sort(order_temp.begin(), order_temp.end(),
        [](const TempOrderTup & lhs, const TempOrderTup & rhs)
        { return std::get<0>(lhs) < std::get<0>(rhs); });
    for (const auto & [depth, obj] : order_temp) {
        std::cout << (obj->name.empty() ? "<NO NAME>" : obj->name) << " depth: " << depth << std::endl;
    }

    std::vector<const tmap::MapObject *> rv;
    rv.reserve(objects.size());
    for (const auto & tuple : order_temp) {
        rv.push_back(std::get<1>(tuple));
    }
    return rv;
}

std::vector<const tmap::MapObject *> get_map_load_order
    (const tmap::MapObject::MapObjectContainer & objcontainer,
     std::map<std::string, const tmap::MapObject *> * namemap)
{
    std::map<std::string, const tmap::MapObject *> fallbackinst;
    return get_map_load_order_(objcontainer, namemap ? *namemap : fallbackinst);
}

VectorD parse_vector(std::string::const_iterator beg, std::string::const_iterator end) {
    using StrIter = std::string::const_iterator;
    VectorD rv;
    auto list = { &rv.x, &rv.y };
    auto itr = list.begin();
    auto v_end = list.end();
    for_split<is_comma>(beg, end,
        [&itr, v_end](StrIter beg, StrIter end)
    {
        trim<is_whitespace>(beg, end);
        if (itr == v_end) {
            throw std::invalid_argument("parse_vector: too many arguments for a 2D vector");
        }
        if (!string_to_number(beg, end, **itr++)) {
            throw std::invalid_argument("parse_vector: non-nummeric argument");
        }
    });
    if (itr != v_end) {
        throw std::invalid_argument("parse_vector: too few arguments for a 2D vector");
    }
    return rv;
}

VectorD parse_vector(const std::string & str)
    { return parse_vector(str.begin(), str.end()); }

namespace {

inline VectorD center_of(const tmap::MapObject & obj)
    { return VectorD(::center_of(obj.bounds)); }

InterpolativePosition::Behavior load_waypoints_behavior(const tmap::MapObject::PropertyMap &);

void load_display_frame(DisplayFrame &, const tmap::MapObject &);

void load_player_start(MapObjectLoader & loader, const tmap::MapObject & obj) {
    auto e = loader.create_entity();
    e.add<PlayerControl>();
    e.add<PhysicsComponent>().reset_state<FreeBody>().location = center_of(obj);

    auto itr = obj.custom_properties.find("sprite-sheet");
    if (itr != obj.custom_properties.end()) {
        auto & animator = e.add<DisplayFrame>().reset<CharacterAnimator>();
        animator.sprite_sheet = loader.load(TypeTag<SpriteSheet>(), itr->second);
    } else {
        add_color_circle(e, random_color(loader.get_rng()));
    }
    e.add<Collector>().collection_offset = VectorD(0, -16);
    e.add<HeadOffset>() = VectorD(0, -32);
    loader.set_player(e);

    auto start_point = loader.create_entity();
    start_point.add<PhysicsComponent>().reset_state<Rect>() = Rect(obj.bounds);

    e.add<ReturnPoint>().ref = start_point;
    e.add<ScriptUPtr>() = std::make_unique<PlayerScript>(e);
}

void load_snake(MapObjectLoader & loader, const tmap::MapObject & obj) {
    auto e = loader.create_entity();
    auto & snake = e.add<Snake>();
    snake.location    = center_of(obj);
    snake.begin_color = random_color(loader.get_rng());
    snake.end_color   = random_color(loader.get_rng());
}

void load_coin(MapObjectLoader & loader, const tmap::MapObject & obj) {
    auto e = loader.create_entity();
    auto & freebody = e.add<PhysicsComponent>().reset_state<FreeBody>();
    freebody.location = center_of(obj);
    add_color_circle(e, random_color(loader.get_rng()));
}

void load_diamond(MapObjectLoader & loader, const tmap::MapObject & obj) {
    auto e = loader.create_entity();
    auto & rect = e.add<PhysicsComponent>().reset_state<Rect>() = Rect(obj.bounds);
    rect.top  -= std::remainder(rect.top , 8.);
    rect.left -= std::remainder(rect.left, 8.);
    load_display_frame(e.add<DisplayFrame>(), obj);
    loader.load_animation(e.add<TriggerBox>(), obj);
}

static constexpr const auto k_set_target_attr = "set-target";

void load_launcher(MapObjectLoader & loader, const tmap::MapObject & obj) {
    using Launcher = TriggerBox::Launcher;
    using TargetedLauncher = TriggerBox::TargetedLauncher;
    static auto round_rect = [](Rect r) {
        r.left   = std::round(r.left  );
        r.top    = std::round(r.top   );
        r.width  = std::round(r.width );
        r.height = std::round(r.height);
        return r;
    };

    auto e = obj.name.empty() ? loader.create_entity() : loader.create_named_entity_for_object();
    e.add<PhysicsComponent>().reset_state<Rect>() = round_rect(Rect(obj.bounds));

    auto do_if_found = make_do_if_found(obj.custom_properties);
    static constexpr const auto k_launch_props_list = "\"launch\", \"boost\", \"set\", \"set-target\"";
    static auto make_can_only_have_one_type_error = []() {
        return std::runtime_error(
            "load_launcher: conflict launch type properties are present, where "
            "there may only be one. They are: "
            + std::string(k_launch_props_list) + ".");
    };

    static auto add_launcher = [](Entity e, Launcher::LaunchType type) {
        if (e.has<TriggerBox>()) throw make_can_only_have_one_type_error();
        e.add<TriggerBox>().reset<Launcher>().type = type;
    };

    const std::string * val_ptr = nullptr;
    do_if_found("launch", [&e, &val_ptr](const std::string & val) {
        add_launcher(e, Launcher::k_detacher);
        val_ptr = &val;
    });
    do_if_found("boost", [&e, &val_ptr](const std::string & val) {
        add_launcher(e, Launcher::k_booster);
        val_ptr = &val;
    });
    do_if_found("set", [&e, &val_ptr](const std::string & val) {
        add_launcher(e, Launcher::k_setter);
        val_ptr = &val;
    });
    do_if_found(k_set_target_attr, [&e, &loader](const std::string & val) {
        if (e.has<TriggerBox>()) throw make_can_only_have_one_type_error();
        e.add<TriggerBox>().reset<TargetedLauncher>().target = loader.find_named_entity(val);
    });

    if (val_ptr) {
        e.get<TriggerBox>().as<Launcher>().launch_velocity = MiniVector(parse_vector(*val_ptr));
    } else {
        assert(e.get<TriggerBox>().state.is_type<TargetedLauncher>());
    }

    load_display_frame(e.add<DisplayFrame>(), obj);
}

const std::vector<std::string> & get_launcher_requirements() {
    static std::vector<std::string> inst = { k_set_target_attr };
    return inst;
}

void load_platform(MapObjectLoader & loader, const tmap::MapObject & obj) {
    auto e = loader.create_entity();
    Surface surface(to_floor_segment(obj.bounds));
    e.add<Platform>().set_surfaces(std::vector<Surface> { surface });

    InterpolativePosition intpos;
    Waypoints waypts;
    intpos.set_behavior(load_waypoints_behavior(obj.custom_properties));
    auto do_if_found = make_do_if_found(obj.custom_properties);
    do_if_found("waypoints", [&waypts, &loader](const std::string & val) {
        waypts = loader.load_waypoints(loader.find_map_object(val));
    });

    if (waypts.has_points()) {
        assert(waypts->size() > 1);
        intpos.set_point_count(waypts->size());
        assert(intpos.point_count() == waypts->size());
    }

    do_if_found("speed", [&intpos](const std::string & val) {
        double spd = 0.;
        if (string_to_number(val, spd))
            intpos.set_speed(spd);
    });
    do_if_found("position", [&intpos](const std::string & val) {
        //if (waypts.waypoints->empty()) return;
        if (intpos.segment_count() == 0) return;
        // should like the position to apply to the entire cycle and not just
        // the current segment
        double x = 0.;
        if (!string_to_number(val, x)) {
            return;
        } else if (x < 0. || x > 1.) {
            return;
        }
        intpos.set_whole_position(x*double(intpos.point_count()));
    });
    if (waypts.has_points()) {
        if (waypts.points().size() > 1) {
            e.add<Waypoints>() = std::move(waypts);
            e.add<InterpolativePosition>() = intpos;
        }
    }
}

void load_wall(MapObjectLoader & loader, const MapObject & obj) {
    auto walle = loader.create_entity();
    walle.add<Platform>().set_surfaces({ Surface(LineSegment(
        obj.bounds.left + obj.bounds.width*0.5, obj.bounds.top,
        obj.bounds.left + obj.bounds.width*0.5, obj.bounds.top + obj.bounds.height)) });

}

static constexpr const auto k_ball_recall_bounds = "recall-bounds";

void load_ball(MapObjectLoader & loader, const MapObject & obj) {
    Item::HoldType hold_type = Item::simple;
    auto do_if_found = make_do_if_found(obj.custom_properties);
    do_if_found("ball-type", [&hold_type](const std::string & val) {
        if (val == "jump-booster") {
            hold_type = Item::jump_booster;
        }
    });

    auto recall_e = loader.create_entity();
    recall_e.add<PhysicsComponent>().reset_state<Rect>() = Rect(obj.bounds);

    auto ball_e = loader.create_entity();
    ball_e.add<PhysicsComponent>().reset_state<FreeBody>().location = center_of(obj);
    ball_e.add<Item>().hold_type = hold_type;
    if (obj.tile_set) {
        load_display_frame(ball_e.add<DisplayFrame>(), obj);
    } else {
        ball_e.add<DisplayFrame>().reset<ColorCircle>().color = sf::Color(200, 100, 100);
    }
    auto & rt_point = ball_e.add<ReturnPoint>();
    rt_point.ref = recall_e;
    do_if_found("recall-time", [&rt_point](const std::string & val) {
        double time = ReturnPoint::k_default_recall_time;
        auto b = val.begin(), e = val.end();
        trim<is_whitespace>(b, e);
        if (string_to_number(b, e, time)) {
            rt_point.recall_max_time = rt_point.recall_time = time;
        } else {
            std::cout << "recall-time was not numeric (value: \"" << val << "\")." << std::endl;
        }
    });
    do_if_found(k_ball_recall_bounds, [&loader, /*&ball_e,*/ &rt_point](const std::string & val) {
        rt_point.recall_bounds = Rect(loader.find_map_object(val)->bounds);
    });
}

void load_basket(MapObjectLoader & loader, const MapObject & obj) {
    auto do_if_found = make_do_if_found(obj.custom_properties);
    std::vector<VectorD> outline;
    do_if_found("outline", [&loader, &outline](const std::string & val) {
        auto * outline_obj = loader.find_map_object(val);
        if (!outline_obj) return;
        if (outline_obj->points.empty()) return;
        outline = convert_vector_to<VectorD>(outline_obj->points);
    });
    using WaypointsPtr = Waypoints::ContainerPtr;
    WaypointsPtr waypts;
    do_if_found("sink-points", [&loader, &waypts](const std::string & val) {
        waypts = loader.load_waypoints(loader.find_map_object(val));
    });
    if (outline.empty() || !waypts) {
        // required properties not found
        return;
    }

    Rect outline_extremes = [&outline]() {
        if (outline.empty()) throw BadBranchException();
        VectorD low ( k_inf,  k_inf);
        VectorD high(-k_inf, -k_inf);
        for (const auto & pt : outline) {
            low.x  = std::min(low.x , pt.x);
            low.y  = std::min(low.y , pt.y);
            high.x = std::max(high.x, pt.x);
            high.y = std::max(high.y, pt.y);
        }
        return Rect(low.x, low.y, high.x - low.x, high.y - low.y);
    } ();

    VectorD scale(double(obj.bounds.width ) / outline_extremes.width ,
                  double(obj.bounds.height) / outline_extremes.height);
    // remove outline translation
    for (auto & pt : outline) {
        pt -= VectorD(outline_extremes.left, outline_extremes.top);
        // scale to bounds
        pt.x *= scale.x;
        pt.y *= scale.y;
        // add obj translation
        pt += VectorD(double(obj.bounds.left), double(obj.bounds.top));
        pt.x = std::round(pt.x);
        pt.y = std::round(pt.y);
    }

    std::vector<Surface> surfaces;
    surfaces.reserve(outline.size());
    for_side_by_side(outline, [&surfaces](VectorD lhs, VectorD rhs) {
        if (are_very_close(lhs, rhs)) return;
        surfaces.emplace_back(LineSegment(lhs, rhs));
    });
    for (const auto & surface : surfaces) {
        assert(!are_very_close(surface.a, surface.b));
    }

    auto basket_e = loader.create_entity();
    basket_e.add<Waypoints>() = std::move(waypts);
    {
    auto & intpos = basket_e.add<InterpolativePosition>();
    intpos.set_speed(50.);
    intpos.set_point_count(basket_e.get<Waypoints>()->size());
    assert(intpos.point_count() == basket_e.get<Waypoints>()->size());
    intpos.target_point(0);
    }
    basket_e.add<Platform>().set_surfaces(std::move(surfaces));

    for (auto surf : basket_e.get<Platform>().surface_view()) {
        assert(!are_very_close(surf.a, surf.b));
    }
    auto script = std::make_unique<BasketScript>();
    do_if_found("wall-to-open", [&loader, &script](const std::string & val) {
        const auto * obj = loader.find_map_object(val);
        if (!obj) return;
        auto wall_e = loader.create_entity();
        wall_e.add<Platform>().set_surfaces(std::vector<Surface> { Surface(LineSegment(
            obj->bounds.left + obj->bounds.width*0.5, obj->bounds.top,
            obj->bounds.left + obj->bounds.width*0.5, obj->bounds.top + obj->bounds.height)) });
        script->set_wall(wall_e);
    });

    basket_e.add<ScriptUPtr>() = std::move(script);
}

void load_checkpoint(MapObjectLoader & loader, const MapObject & obj) {
    auto checkpoint = loader.create_entity();
    checkpoint.add<PhysicsComponent>().reset_state<Rect>() = Rect(obj.bounds);
    checkpoint.add<TriggerBox>().reset<TriggerBox::Checkpoint>();
}

void load_scale_pivot(MapObjectLoader & loader, const MapObject & obj) {
    if (obj.name.empty()) {
        throw std::runtime_error("Scale pivot must have a name.");
    }
    auto e = loader.create_named_entity_for_object();
    e.add<PhysicsComponent>().reset_state<Rect>() = Rect(obj.bounds);
    e.add<ScriptUPtr>() = std::make_unique<ScalePivotScriptN>(e);
}

std::pair<std::string, Entity> load_scale_part
    (MapObjectLoader & loader, const MapObject & obj)
{
    auto itr = obj.custom_properties.find("scale-pivot");
    if (itr == obj.custom_properties.end()) {
        throw std::runtime_error("Scale part must name its pivot.");
    }
    auto e = loader.create_entity();
    // will be removed by the script on creation
    e.add<PhysicsComponent>().reset_state<Rect>() = Rect(obj.bounds);

    return std::make_pair(itr->second, e);
}

void load_scale_left(MapObjectLoader & loader, const MapObject & obj) {
    auto [name, e] = load_scale_part(loader, obj);
    auto pivot = loader.find_named_entity(name);
    // dynamic_cast here is expected to alway work
    // unless I messed up my code
    if (dynamic_cast<ScalePivotScriptN &>(*pivot.get<ScriptUPtr>())
        .set_left(e).is_finished())
    {
        pivot.remove<ScriptUPtr>();
    }
}

void load_scale_right(MapObjectLoader & loader, const MapObject & obj) {
    auto [name, e] = load_scale_part(loader, obj);
    auto pivot = loader.find_named_entity(name);
    if (dynamic_cast<ScalePivotScriptN &>(*pivot.get<ScriptUPtr>())
        .set_right(e).is_finished())
    {
        pivot.remove<ScriptUPtr>();
    }
}

const std::vector<std::string> & get_scale_part_requirements() {
    static const std::vector<std::string> inst { "scale-pivot" };
    return inst;
}

void load_rectangle(MapObjectLoader & loader, const MapObject & obj) {
    auto e = loader.create_named_entity_for_object();
    e.add<PhysicsComponent>().reset_state<Rect>() = Rect(obj.bounds);
}

template <typename T>
LineSegment to_floor_segment(const sf::Rect<T> & rect) {
    return LineSegment(
            rect.left             , rect.top + rect.height*0.5,
            rect.left + rect.width, rect.top + rect.height*0.5);
}

// ----------------------------------------------------------------------------

void load_display_frame(DisplayFrame & dframe, const tmap::MapObject & obj) {
    if (obj.tile_set) {
        auto & si = dframe.reset<SingleImage>();
        si.texture = &obj.tile_set->texture();
        si.texture_rectangle = obj.tile_set->texture_rectangle(obj.local_tile_id);
    }
}

InterpolativePosition::Behavior load_waypoints_behavior(const tmap::MapObject::PropertyMap & props) {
    using IntPos = InterpolativePosition;
    static constexpr const auto k_default_behavior = IntPos::k_cycles;
    auto itr = props.find("cycle-behavior");
    if (props.end() == itr) return k_default_behavior;
    if (itr->second == "cycles") {
        return IntPos::k_cycles;
    } else if (itr->second == "idle") {
        return IntPos::k_idle;
    } else if (itr->second == "foreward" || itr->second == "forewards") {
        return IntPos::k_foreward;
    }
    // log invalid argument
    return k_default_behavior;
}

} // end of <anonymous> namespace
