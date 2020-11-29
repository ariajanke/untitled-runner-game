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

#include <cstring>

namespace {

using MapObject = tmap::MapObject;
using MapLoaderFunctionMap = std::map<std::string, MapObjectLoaderFunction>;

const MapLoaderFunctionMap & get_loader_functions();

void load_player_start(MapObjectLoader &, const MapObject &);
void load_snake       (MapObjectLoader &, const MapObject &);
void load_coin        (MapObjectLoader &, const MapObject &);
void load_diamond     (MapObjectLoader &, const MapObject &);
void load_launcher    (MapObjectLoader &, const MapObject &);
void load_platform    (MapObjectLoader &, const MapObject &);
void load_waypoints   (MapObjectLoader &, const MapObject &);
void load_wall        (MapObjectLoader &, const MapObject &);
void load_ball        (MapObjectLoader &, const MapObject &);

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
    std::make_pair("waypoints"     , load_waypoints     ),
    std::make_pair("wall"          , load_wall          ),
    std::make_pair("ball"          , load_ball          ),

    std::make_pair("scale-left"    , load_scale_left    ),
    std::make_pair("scale-right"   , load_scale_right   ),
    std::make_pair("scale-pivot"   , load_scale_pivot   )
};

const auto k_reserved_objects = {
    k_line_map_transition_object
};

template <typename T>
LineSegment to_floor_segment(const sf::Rect<T> &);

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

MapObjectLoader::MapObjectLoader() {
    CachedLoader<SpriteSheet>::set_loader_function(TypeTag<SpriteSheet>(), []
        (const std::string & filename, SpriteSheet & sptsheet)
    {
        sptsheet.load_from_file(filename);
    });
    CachedLoader<std::vector<VectorD>, WaypointsTag>::set_loader_function
        (WaypointsTag(), [](const std::string &, std::vector<VectorD> &) {});
}

/* vtable anchor */ MapObjectLoader::~MapObjectLoader() {}

MapObjectLoaderFunction get_loader_function(const std::string & typekey) {
    auto itr = get_loader_functions().find(typekey);
    if (itr == get_loader_functions().end()) {
        return nullptr;
    }
    return itr->second;
}

namespace {

inline VectorD center_of(const tmap::MapObject & obj) {
    return VectorD(obj.bounds.left , obj.bounds.top   ) +
           VectorD(obj.bounds.width, obj.bounds.height)*0.5;
}

VectorD parse_vector(const std::string &);

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
    e.add<PhysicsComponent>().reset_state<Rect>() = Rect(obj.bounds);
    e.add<Item>().diamond = 1;
    load_display_frame(e.add<DisplayFrame>(), obj);
}

void load_launcher(MapObjectLoader & loader, const tmap::MapObject & obj) {
    auto e = loader.create_entity();
    e.add<PhysicsComponent>().reset_state<Rect>() = Rect(obj.bounds);
    auto & launcher = e.add<Launcher>();
    auto & props    = obj.custom_properties;
    auto itr = props.find("launch");
    if (itr != props.end()) {
        launcher.launch_velocity = parse_vector(itr->second);
        launcher.detaches_entity = true;
    } else if ((itr = props.find("boost")) != props.end()) {
        launcher.launch_velocity = parse_vector(itr->second);
        launcher.detaches_entity = false;
    } else {
        throw std::invalid_argument("load_launcher: either \"launch\" or \"boost\" attribute required.");
    }
    load_display_frame(e.add<DisplayFrame>(), obj);
}

void load_platform(MapObjectLoader & loader, const tmap::MapObject & obj) {
    auto e = loader.create_entity();
    Surface surface(to_floor_segment(obj.bounds));
    e.add<Platform>().set_surfaces(std::vector<Surface> { surface });
    auto itr = obj.custom_properties.find("waypoints");
    if (itr != obj.custom_properties.end()) {
        e.get<Platform>().waypoints = loader.load(WaypointsTag(), itr->second);
        e.get<Platform>().waypoint_number = 0;
    }
    itr = obj.custom_properties.find("speed");
    if (itr != obj.custom_properties.end()) {
        string_to_number(itr->second, e.get<Platform>().speed);
    }
    itr = obj.custom_properties.find("position");
    if (itr != obj.custom_properties.end()) {
        // should like the position to apply to the entire cycle and not just
        // the current segment
        string_to_number(itr->second, e.get<Platform>().position);
    }
#   if 0
    e.add<ScriptUPtr>() = std::make_unique<PrintOutLandingsDepartingsScript>();
#   endif
}

void load_waypoints(MapObjectLoader & loader, const MapObject & obj) {
    std::vector<VectorD> waypoints;
    waypoints.reserve(obj.points.size());
    for (auto r : obj.points) {
        waypoints.push_back(VectorD(r));
    }
    auto sptr = loader.load(WaypointsTag(), obj.name);
    sptr->swap(waypoints);
    // sptr isn't living anywhere...
    // this is magic...
    auto e = loader.create_entity();
    e.add<Platform>().waypoints = sptr;
    // deletes itself after map loading
    e.request_deletion();
}

void load_wall(MapObjectLoader & loader, const MapObject & obj) {
    auto walle = loader.create_entity();
    walle.add<Platform>().set_surfaces({ Surface(LineSegment(
        obj.bounds.left + obj.bounds.width*0.5, obj.bounds.top,
        obj.bounds.left + obj.bounds.width*0.5, obj.bounds.top + obj.bounds.height)) });

}

void load_ball(MapObjectLoader & loader, const MapObject & obj) {
    Item::HoldType hold_type = Item::simple;
    auto itr = obj.custom_properties.find("ball-type");
    if (itr != obj.custom_properties.end()) { if (itr->second == "jump-booster") {
        hold_type = Item::jump_booster;
    }}
    auto recall_e = loader.create_entity();
    recall_e.add<PhysicsComponent>().reset_state<Rect>() = Rect(obj.bounds);

    auto ball_e = loader.create_entity();
    ball_e.add<PhysicsComponent>().reset_state<FreeBody>().location = center_of(obj);
    ball_e.add<Item>().hold_type = hold_type;
    if (obj.texture) {
        load_display_frame(ball_e.add<DisplayFrame>(), obj);
    } else {
        ball_e.add<DisplayFrame>().reset<ColorCircle>().color = sf::Color(200, 100, 100);
    }
    ball_e.add<ReturnPoint>().ref = recall_e;
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

inline bool is_comma(char c) { return c == ','; }

void load_display_frame(DisplayFrame & dframe, const tmap::MapObject & obj) {
    if (obj.texture) {
        auto & si = dframe.reset<SingleImage>();
        si.texture = obj.texture;
        si.texture_rectangle = obj.texture_bounds;
    }
}

VectorD parse_vector(const std::string & str) {
    VectorD rv;
    auto list = { &rv.x, &rv.y };
    auto itr = list.begin();
    auto v_end = list.end();
    for_split<is_comma>(str.c_str(), str.c_str() + str.length(),
        [&itr, v_end](const char * beg, const char * end)
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

} // end of <anonymous> namespace
