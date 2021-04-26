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
    using ConstTileSetPtr = tmap::MapObject::ConstTileSetPtr;
    std::vector<int> tile_ids;
    ConstTileSetPtr tileset = nullptr;
    double time_per_frame = 0.;
};

struct ItemCollectionInfo : public ItemCollectionAnimation {
    int diamond_quantity = 0;
};

using ItemCollectionSharedPtr = std::shared_ptr<const ItemCollectionInfo>;

struct Item {
    enum HoldType {
        simple, jump_booster, heavy, bouncy, platform_breaker,
        run_booster, crate, not_holdable
    };
    static constexpr const int k_hold_type_count = not_holdable;
    HoldType hold_type = not_holdable;
};

struct DecoItem {};

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

class MiniVector {
public:
    using BaseType = uint8_t;
    static constexpr const double k_error      = ::k_error;
    static constexpr const double k_scale      = 12. / sizeof(BaseType);
    static constexpr const double k_max        = k_scale*double(std::numeric_limits<BaseType>::max());
    static constexpr const double k_angle_end  = double(std::numeric_limits<BaseType>::max()) + 1.;
    static constexpr const double k_angle_step = (2.*k_pi) / k_angle_end;

    MiniVector() {}
    explicit MiniVector(VectorD);
    VectorD expand() const noexcept;

private:
    static const VectorD k_unit_start;
    uint8_t m_dir = 0;
    BaseType m_mag = 0;
};
#if 0
template <typename ... CompleteComponents>
struct DefineTupleSystemFor {
    using ContainerView = typename ecs::System<CompleteComponents ...>::ContainerView;
    using EntityType    = typename ecs::Entity<CompleteComponents ...>;
    template <typename ... Types>
    using Tuple = std::tuple<Types...>;

    template <typename ... Types>
    static auto make_component_tuple(EntityType &) {
        return std::make_tuple(/* empty tuple */);
    }

    template <typename T, typename ... Types>
    static auto make_component_tuple(EntityType & e) {
        return std::tuple_cat( std::make_tuple(e.template get<T>()),
                               make_component_tuple<Types...>(e));
    }

    template <typename T, typename ... Types>
    class Singles : public ecs::System<CompleteComponents...> {
    public:
        using TupleType = std::tuple<Types...>;

        virtual void update(TupleType &) = 0;

        void update(const ContainerView & view) final {
            for (auto & e : view) {
                update( make_component_tuple<T, Types...>(e) );
            }
        }
    };

};
#endif
struct TriggerBox {
    struct Checkpoint {};
    struct HarmfulObject {};
    struct Launcher {
        MiniVector launch_velocity = MiniVector(VectorD(0, -467));
        bool detaches = true;
    };

    using StateType = MultiType<HarmfulObject, Checkpoint, Launcher, ItemCollectionSharedPtr>;

    template <typename T>
    T * ptr() { return state.as_pointer<T>(); }

    template <typename T>
    const T * ptr() const { return state.as_pointer<T>(); }

    template <typename T>
    T & as() { return state.as<T>(); }

    template <typename T>
    const T & as() const { return state.as<T>(); }

    template <typename T>
    T & reset() { return state.reset<T>(); }

    StateType state;
};

struct TriggerBoxSubjectHistory {
public:
    friend class TriggerBoxSubjectHistoryAtt;
    static const VectorD k_no_location;
    VectorD last_location() const noexcept { return m_last_location; }

private:
    VectorD m_last_location = k_no_location;
};

class TriggerBoxSystem;
class TriggerBoxSubjectHistoryAtt {
    friend class TriggerBoxSystem;
    static void set_location(TriggerBoxSubjectHistory & history, VectorD r)
        { history.m_last_location = r; }
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
