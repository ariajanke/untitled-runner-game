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

#include "Defs.hpp"

#include <common/MultiType.hpp>

#include <tmap/MapObject.hpp>

#include <memory>

struct ItemCollectionAnimation {
    using TileSetPtr = tmap::MapObject::TileSetPtr;
    std::vector<int> tile_ids;
    TileSetPtr tileset = nullptr;
    double time_per_frame = 0.;
};

struct Item {
    enum HoldType {
        simple, jump_booster, heavy, bouncy, platform_breaker,
        run_booster, crate, not_holdable
    };
    static constexpr const int k_hold_type_count = not_holdable;
    int      diamond   = 0;
    HoldType hold_type = not_holdable;
    std::shared_ptr<ItemCollectionAnimation> collection_animation = nullptr;
};

struct Collector {
    friend class ItemPickerPriv;
    static const VectorD k_no_location;
    VectorD   last_location = k_no_location; // last location of the entity
    VectorD   collection_offset;
    int       diamond = 0;
    EntityRef held_object() const { return m_held_object; }
private:
    EntityRef m_held_object;
};

struct Lifetime {
    double value = 60.;
};

struct Launcher {
    VectorD launch_velocity = VectorD(0, -467);
    bool detaches_entity = true;
};

struct LauncherSubjectHistory {
    VectorD last_location;
};

struct Snake {
    static const constexpr int k_default_instance_count = 25;
    VectorD location;             // set then left
    int total_instances     = k_default_instance_count; // set then left
    int instances_remaining = k_default_instance_count;
    double until_next_spawn = 0.025; // set then left
    double elapsed_time     = 0.;
    sf::Color begin_color, end_color; // set then left
};

struct ReturnPoint {
    static constexpr const double k_default_recall_time = 1.;
    EntityRef ref;
    Rect recall_bounds     = Rect(-k_inf, -k_inf, k_inf, k_inf);
    double recall_max_time = k_default_recall_time;
    double recall_time     = k_default_recall_time;
};

struct HeadOffset : public VectorD {
    HeadOffset & operator = (const VectorD & rhs) {
        *static_cast<VectorD *>(this) = rhs;
        return *this;
    }
};

// there maybe a significant amount of data associated with the player
// I'm not sure if this is a point where all I need is one system and one
// component to handle all of it, or if I need to use a "script"
struct PlayerControl {
    enum Direction : uint8_t {
        k_neither_dir,
        // one direction
        k_left_only, k_right_only,
        // both directions
        k_left_last, k_right_last
    };

    enum SimpleDirection : uint8_t
        { k_left, k_right };

    bool jump_held    = false;
    bool grabbing     = false;
    bool will_release = false;
    bool releasing    = false;

    Direction       direction      = k_neither_dir;
    SimpleDirection last_direction = k_right;

    // this is more physics oriented
    double jump_time = 0.;
};


double direction_of(const PlayerControl &);

void press_left (PlayerControl &);
void press_right(PlayerControl &);

void release_left (PlayerControl &);
void release_right(PlayerControl &);

enum class ControlMove {
    jump,
    move_left,
    move_right,
    use
};

struct PressEvent {
    PressEvent(ControlMove cm_): button(cm_) {}
    ControlMove button;
};

struct ReleaseEvent {
    ReleaseEvent(ControlMove cm_): button(cm_) {}
    ControlMove button;
};

using ControlEvent = MultiType<PressEvent, ReleaseEvent>;
constexpr const int k_press_event   = ControlEvent::GetTypeId<PressEvent  >::k_value;
constexpr const int k_release_event = ControlEvent::GetTypeId<ReleaseEvent>::k_value;

class Script;

using ScriptUPtr = std::unique_ptr<Script>;
