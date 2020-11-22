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

#include "../Components.hpp"

namespace sf { class RenderTarget; }

class System : public EntityManager::SystemType {
public:
    virtual void setup() {}
};

class TimeAware {
public:
    void set_elapsed_time(double et) { m_et = et; }
protected:
    double elapsed_time() const noexcept { return m_et; }
private:
    double m_et = 0.;
};

class LineMap;
class LineMapLayer;

class MapAware {
public:
    void assign_map(const LineMap & lmap) { m_lmap = &lmap; }
protected:
    MapAware() {}
    const LineMap & line_map() const { return *m_lmap; }
    const LineMapLayer & get_map_layer(const Entity & e) {
        return get_map_layer(e.get<PhysicsComponent>().active_layer);
    }
    const LineMapLayer & get_map_layer(const Layer &);
private:
    const LineMap * m_lmap = nullptr;
};

class RenderTargetAware {
public:
    virtual ~RenderTargetAware();
    virtual void render_to(sf::RenderTarget &) = 0;
protected:
    RenderTargetAware() {}
};
#if 0
// downside to this design
// can't process multiple landings/departings for one system per frame
// so most of the hit is felt with the core physics system
// I don't see this becoming a major problem, but it is a "real" limitation
class PhysicsHistoryEventProcessor final {
public:
    void record_landing(Entity, VectorD vel, EntityRef);
    void record_departing(Entity, EntityRef);
    void process_events();
private:
    struct Landing {
        EntityRef hitter;
        VectorD velocity;
    };
    struct Departing {
        EntityRef departer;
    };
    using Event = MultiType<Landing, Departing>;
    std::vector<std::pair<Entity, Event>> m_events;
};

class PhysicsHistoryWatcher final {
public:
    PhysicsHistoryWatcher() {}
    PhysicsHistoryWatcher(Entity, PhysicsHistoryEventProcessor *);
    ~PhysicsHistoryWatcher() noexcept(false);
private:
    // PhysicsHistory m_history; // we'll store it here eventually
    Entity m_entity;
    PhysicsHistoryEventProcessor * m_sink = nullptr;
};

class PhysicsHistoryAware {
public:
    void assign_event_sink(PhysicsHistoryEventProcessor & sink) { m_sink = &sink; }
protected:
    using WatchContainer = std::vector<PhysicsHistoryWatcher>;
    struct WatchDeleter {
        void operator () (WatchContainer * cont) const noexcept {
            if (cont) cont->clear();
        }
    };
    using WatcherUPtr = std::unique_ptr<WatchContainer, WatchDeleter>;

    PhysicsHistoryAware() {}
    virtual ~PhysicsHistoryAware();

    [[nodiscard]] WatcherUPtr make_watchers(const System::ContainerView & view) {
        if (!m_watchers.empty()) throw std::runtime_error("");
        m_watchers.reserve(view.end() - view.begin());
        for (auto e : view) {
            if (!e.has<PhysicsComponent>()) continue;
            m_watchers.emplace_back(make_watcher_for<ScriptUPtr, PhysicsComponent>(e));
        }
        return WatcherUPtr(&m_watchers);
    }

private:

    template <typename ... Types>
    [[nodiscard]] PhysicsHistoryWatcher make_watcher_for(Entity);

    PhysicsHistoryEventProcessor * m_sink;
    std::vector<PhysicsHistoryWatcher> m_watchers;
};

template <typename ... Types>
PhysicsHistoryWatcher PhysicsHistoryAware::make_watcher_for(Entity e) {
    using GivenTypes = TypeList<Types...>;
    static_assert(
        GivenTypes::template HasType<ScriptUPtr      >::k_value &&
        GivenTypes::template HasType<PhysicsComponent>::k_value ,
        "Both Script and PhysicsComponent access is needed by this function.");
    return PhysicsHistoryWatcher(e, m_sink);
}
#endif
