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
