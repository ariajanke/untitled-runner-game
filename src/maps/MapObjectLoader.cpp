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

#include <common/StringUtil.hpp>

#include <iostream>

#include <cstring>
#include <cassert>

namespace {

using MapObject = tmap::MapObject;
using MapLoaderFunctionMap = std::map<std::string, MapObjectLoaderFunction>;

const MapLoaderFunctionMap & get_loader_functions();

void load_player_start (MapObjectLoader &, const MapObject &);
void load_snake        (MapObjectLoader &, const MapObject &);
void load_coin         (MapObjectLoader &, const MapObject &);
void load_diamond      (MapObjectLoader &, const MapObject &);
void load_launcher     (MapObjectLoader &, const MapObject &);
void load_platform     (MapObjectLoader &, const MapObject &);
void load_wall         (MapObjectLoader &, const MapObject &);
void load_ball         (MapObjectLoader &, const MapObject &);
void load_recall_bounds(MapObjectLoader &, const MapObject &);
void load_basket       (MapObjectLoader &, const MapObject &);
void load_checkpoint   (MapObjectLoader &, const MapObject &);

void load_scale_pivot (MapObjectLoader &, const MapObject &);
void load_scale_left  (MapObjectLoader &, const MapObject &);
void load_scale_right (MapObjectLoader &, const MapObject &);

const auto k_loader_functions = {
    std::make_pair("player-start"  , load_player_start  ),
    std::make_pair("snake"         , load_snake         ),
    std::make_pair("coin"          , load_coin          ),
    std::make_pair("diamond"       , load_diamond       ),
    std::make_pair("launcher"      , load_launcher      ),
    std::make_pair("platform"      , load_platform      ),
    std::make_pair("wall"          , load_wall          ),
    std::make_pair("ball"          , load_ball          ),
    std::make_pair("recall-bounds" , load_recall_bounds ),
    std::make_pair("scale-left"    , load_scale_left    ),
    std::make_pair("scale-right"   , load_scale_right   ),
    std::make_pair("scale-pivot"   , load_scale_pivot   ),
    std::make_pair("basket"        , load_basket        ),
    std::make_pair("checkpoint"    , load_checkpoint    ),
    std::make_pair("balloon"       , BalloonScript::load_balloon)
};

const auto k_reserved_objects = {
    k_line_map_transition_object
};

template <typename T>
LineSegment to_floor_segment(const sf::Rect<T> &);

inline bool is_comma(char c) { return c == ','; }

}  // end of <anonymous> namespace

void ScaleLoader::register_scale_left_part (const std::string & name, Entity e) {
    m_scale_records[name].left = e;
    check_complete_scale_record(m_scale_records[name]);
}

void ScaleLoader::register_scale_right_part(const std::string & name, Entity e) {
    m_scale_records[name].right = e;
    check_complete_scale_record(m_scale_records[name]);
}

void ScaleLoader::register_scale_pivot_part(const std::string & name, Entity e) {
    m_scale_records[name].pivot = e;
    check_complete_scale_record(m_scale_records[name]);
}

/* private */ void ScaleLoader::check_complete_scale_record
    (const ScaleRecord & record)
{
    if (!record.left || !record.right || !record.pivot) return;
    ScalePivotScript::add_pivot_script_to(record.pivot, record.left, record.right);
}

// ----------------------------------------------------------------------------

// does nothing if there's an active exception
RecallBoundsLoader::~RecallBoundsLoader() {
    if (std::uncaught_exceptions() > 0) return;
    for (auto & [name, record] : m_records) {
        (void)name;
        for (auto & recallable : record.recallables) {
            recallable.get<ReturnPoint>().recall_bounds = record.bounds;
        }
    }
}

void RecallBoundsLoader::register_recallable
    (const std::string & bounds_entity_name, Entity e)
{ m_records[bounds_entity_name].recallables.push_back(e); }

void RecallBoundsLoader::register_bounds
    (const std::string & bounds_entity_name, const Rect & bounds)
{
    auto itr = m_records.find(bounds_entity_name);
    if (itr != m_records.end()) { if (itr->second.bounds != ReturnPoint().recall_bounds) {
        std::cout << "Warning: dupelicate recall bounds entity by name \""
                  << bounds_entity_name << "\", this entity will be ignored.";
        return;
    }}
    m_records[bounds_entity_name].bounds = bounds;
}

// ----------------------------------------------------------------------------

void CachedItemAnimations::load_animation
    (TriggerBox & tbox, const tmap::MapObject & obj)
{
    tbox.reset<ItemCollectionSharedPtr>() = load(obj);
}

std::shared_ptr<const ItemCollectionInfo> CachedItemAnimations::load(const tmap::MapObject & obj) {
    auto & ptr = m_map[obj.tile_set->convert_to_gid(obj.local_tile_id)];
    if (!ptr.expired()) return ptr.lock();

    const auto * props = obj.tile_set->properties_on(obj.local_tile_id);
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

MapObjectLoaderFunction get_loader_function(const std::string & typekey) {
    auto itr = get_loader_functions().find(typekey);
    if (itr == get_loader_functions().end()) {
        return nullptr;
    }
    return itr->second;
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

//VectorD parse_vector(const std::string &);

void load_display_frame(DisplayFrame &, const tmap::MapObject &);

const MapLoaderFunctionMap & get_loader_functions() {
    using RtError = std::runtime_error;
    static MapLoaderFunctionMap rv;
    if (!rv.empty()) { return rv; }
    // verify no conflicts with reserved objects
    for (auto & pair : k_loader_functions) {
        for (auto resname : k_reserved_objects) {
            if (strcmp(pair.first, resname) != 0) { continue; }
            throw RtError("get_loader_functions: naming conflict exists with "
                          "reserved map object names.");
        }
    }

    for (auto & pair : k_loader_functions) {
        auto itr = rv.find(pair.first);
        if (itr != rv.end()) {
            throw RtError("get_loader_functions: ambiguous loader function "
                          "name: \"" + std::string(pair.first) + "\"");
        }
        rv[pair.first] = pair.second;
    }
    return rv;
}

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

void load_launcher(MapObjectLoader & loader, const tmap::MapObject & obj) {

    {
    MiniVector mv(VectorD(0, -467));
    auto v = mv.expand();
    assert(magnitude(v - VectorD(0, -467)) < MiniVector::k_scale*0.5);
    }

    auto e = loader.create_entity();
    e.add<PhysicsComponent>().reset_state<Rect>() = Rect(obj.bounds);
    auto & launcher = e.add<TriggerBox>().reset<TriggerBox::Launcher>();
    auto & props    = obj.custom_properties;
    auto itr = props.find("launch");
    if (itr != props.end()) {
        launcher.launch_velocity = MiniVector(parse_vector(itr->second));
        launcher.detaches = true;
    } else if ((itr = props.find("boost")) != props.end()) {
        launcher.launch_velocity = MiniVector(parse_vector(itr->second));
        launcher.detaches = false;
    } else {
        throw std::invalid_argument("load_launcher: either \"launch\" or \"boost\" attribute required.");
    }
    load_display_frame(e.add<DisplayFrame>(), obj);
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
    do_if_found("recall-bounds", [&loader, &ball_e](const std::string & val) {
        loader.register_recallable(val, ball_e);
    });
}

void load_recall_bounds(MapObjectLoader & loader, const MapObject & obj) {
    if (obj.name.empty()) {
        std::cout << "recall-bounds object is unnamed." << std::endl;
        return;
    }
    loader.register_bounds(obj.name, Rect(obj.bounds));
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
    auto e = loader.create_entity();
    e.add<PhysicsComponent>().reset_state<Rect>() = Rect(obj.bounds);
    loader.register_scale_pivot_part(obj.name, e);
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
    loader.register_scale_left_part(name, e);
}

void load_scale_right(MapObjectLoader & loader, const MapObject & obj) {
    auto [name, e] = load_scale_part(loader, obj);
    loader.register_scale_right_part(name, e);
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
