/****************************************************************************

    Copyright 2021 Aria Janke

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

#include "GraphicsDrawer.hpp"
#include "TreeGraphics.hpp"
#include "Flower.hpp"

#include <set>

class ForestDecor final : public MapDecorDrawer {
public:
    struct TreeParameters : public PlantTree::CreationParams {
        TreeParameters() {}
        TreeParameters(VectorD location_, std::default_random_engine & rng_):
            CreationParams(PlantTree::generate_params(rng_)),
            location(location_)
        {}
        VectorD location;
    };

    struct FutureTree {
        virtual bool is_ready() const = 0;
        virtual bool is_done() const = 0;
        virtual PlantTree get_tree() = 0;
    };

    struct FutureTreeMaker {
        using TreeParameters = ::ForestDecor::TreeParameters;
        virtual ~FutureTreeMaker() {}

        virtual std::unique_ptr<FutureTree> make_tree(const TreeParameters &) = 0;
    };

    struct Updatable {
        virtual ~Updatable() {}
        virtual void update(double) = 0;
    };

    ~ForestDecor() override;
#   if 0
    void load_map(const tmap::TiledMap & tmap, MapObjectLoader &) override;
#   endif
    void render_front(sf::RenderTarget &) const override;

    void render_back(sf::RenderTarget &) const override;

    void update(double et) override;

private:
    static constexpr const bool k_use_multithreaded_tree_loading = true;

    std::unique_ptr<TempRes> prepare_map_objects(const tmap::TiledMap & tmap, MapObjectLoader &) override;

    void prepare_map(tmap::TiledMap &, std::unique_ptr<TempRes>) override;

    void load_map_vegetation(const tmap::TiledMap &, MapObjectLoader &);
    std::unique_ptr<TempRes> load_map_waterfalls(const tmap::TiledMap &);

    void plant_new_flower(std::default_random_engine &, VectorD, MapObjectLoader &);

    void plant_new_future_tree(std::default_random_engine &, VectorD, MapObjectLoader &);

    std::vector<Flower> m_flowers;
    std::vector<PlantTree> m_trees;
    std::vector<std::pair<std::unique_ptr<FutureTree>, Entity>> m_future_trees;

    std::set<std::shared_ptr<Updatable>> m_updatables;

    std::unique_ptr<FutureTreeMaker> m_tree_maker;
};
