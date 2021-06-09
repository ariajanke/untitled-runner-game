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

#include "GenBuiltinTileSet.hpp"
#include "Defs.hpp"

#include <common/SubGrid.hpp>
#include <common/Util.hpp>
#include <common/MultiType.hpp>

#include <random>
#include <future>
#include <thread>
#include <iostream>

#include <cstring>
#include <cassert>

namespace {

using cul::SubGrid;
using cul::ConstSubGrid;
using sf::Color;

using cul::make_sub_grid;

static constexpr const int   k_rest_of_grid = cul::k_rest_of_grid;
static constexpr const int   k_tile_size    = 16;
static           const Color k_transparency = Color(0, 0, 0, 0);

const Grid<Color> & get_base_checkerboard();

// assumes four way symetric
void add_border(SubGrid<Color>);

void cut_circle(SubGrid<Color>, bool as_hole);

template <typename T>
Grid<T> extend(const Grid<T> & gin, int wmul, int hmul);

void cut_diamond(SubGrid<Color>, bool as_hole);

Grid<Color> push_down(ConstSubGrid<Color>, int x_line, int y_amount);
#if 0
void add_grass(SubGrid<Color>, std::default_random_engine &);
#endif
void add_grass(SubGrid<Color> dest, SubGrid<Color> source, std::default_random_engine &);
void add_grass(SubGrid<Color> dest, ConstSubGrid<Color> source, std::default_random_engine &);

inline void add_grass(SubGrid<Color> single_target, std::default_random_engine & rng) {
    add_grass(single_target, single_target, rng);
}

Grid<Color> gen_flats(std::default_random_engine &);

Grid<Color> gen_all_indents();

Grid<Color> gen_waterfall(int w, int h, int pal_rotation_idx, int x_);


class Slopes {
public:
    static constexpr const bool k_use_mutlithreading = true;
    using GrassMappings = std::vector<std::tuple<VectorI, VectorI>>;
    struct MergeInfo {
    };

    virtual ~Slopes() {}

    virtual void do_cuts(SubGrid<Color> ground_target, SubGrid<Color> grass_target) = 0;
    virtual void wait_and_merge(SubGrid<Color> target, MergeInfo &) = 0;
    virtual std::unique_ptr<Slopes> clone() const = 0;

    using Task = std::tuple<std::unique_ptr<Slopes>, VectorI, int, int>;

    static Task copy_task(const Task & task) {
        using std::get;
        return std::make_tuple( get<0>(task)->clone(),
                                get<1>(task), get<2>(task), get<3>(task));
    }

    template <void (*cut_f)(SubGrid<Color>, bool), bool kt_as_hole>
    static Task make_task(VectorI r, int width, int height);

    template <void (*cut_f)(SubGrid<Color>, bool)>
    static View<const Task *> get_tasks_for();

protected:
    static void verify_dims(ConstSubGrid<Color> subg) {
        assert(subg.width() % k_tile_size == 0 && subg.height() % k_tile_size == 0);
    }

    static void fill_with_checkerboard(SubGrid<Color> target) {
        verify_dims(target);
        static const auto & k_base = get_base_checkerboard();
        for (int y = 0; y != target.height(); y += k_tile_size) {
        for (int x = 0; x != target.width (); x += k_tile_size) {
            auto sub = make_sub_grid(target, VectorI(x, y), k_tile_size, k_tile_size);
            std::copy(k_base.begin(), k_base.end(), sub.begin());
        }}
    }

    static void clear_only_full_checkerboards(SubGrid<Color> target) {
        verify_dims(target);
        static const auto & k_base = get_base_checkerboard();
        for (int y = 0; y != target.height(); y += k_tile_size) {
        for (int x = 0; x != target.width (); x += k_tile_size) {
            auto sub = make_sub_grid(target, VectorI(x, y), k_tile_size, k_tile_size);
            if (!std::equal(k_base.begin(), k_base.end(), sub.begin())) continue;
            std::fill(sub.begin(), sub.end(), k_transparency);
        }}
    }

    template <bool kt_as_hole>
    static VectorI grass_bounds_offset() {
        return VectorI(0, kt_as_hole ? 0 : k_tile_size);

    }

    static bool is_transparency(Color c) { return c == k_transparency; }
};

template <void (*cut_f)(SubGrid<Color>, bool), bool kt_as_hole>
class CutsSlopes : public Slopes {
protected:

    void do_cuts_task(SubGrid<Color> ground_target, SubGrid<Color> grass_target) {
        static const bool k_is_circle = cut_f == cut_circle;
        static const bool k_is_diamond = cut_f == cut_diamond;
        auto cut_target = make_sub_grid(ground_target,
            grass_bounds_offset<kt_as_hole>(),
            k_rest_of_grid,
            ground_target.height() - k_tile_size - (k_is_diamond ? k_tile_size : 0));
        if (k_is_circle) {
            fill_with_checkerboard(ground_target);
        } else if (k_is_diamond) {
            fill_with_checkerboard(cut_target);
        } else {
            throw BadBranchException();
        }
        cut_f(cut_target, kt_as_hole);
        if (k_is_circle) {
            add_border(cut_target);
        } else if (k_is_diamond) {
            // post cut push down
            do_diamond_push_down(ground_target);
            // add jagged portion
        } else {
            throw BadBranchException();
        }

        auto subrect = grass_bounds(ground_target.width(), ground_target.height());
        VectorI r(subrect.left, subrect.top);
        add_grass(make_sub_grid(grass_target , r, subrect.width, subrect.height),
                  make_sub_grid(ground_target, r, subrect.width, subrect.height),
                  m_rng);

        clear_only_full_checkerboards(ground_target);
        m_grass_mappings = compute_grass_mappings(ground_target, grass_target);
    }

private:
    static void do_diamond_push_down(SubGrid<Color> ground_target) {
        auto h = (ground_target.height() - k_tile_size*2) / 2;
        auto src_move = make_sub_grid(ground_target, VectorI(0, h +   k_tile_size), k_rest_of_grid, h);
        auto dst_move = make_sub_grid(ground_target, VectorI(0, h + 2*k_tile_size), k_rest_of_grid, h);
        assert(src_move.width() == dst_move.width() && src_move.height() == dst_move.height());
        // reverse iterators don't seem to work =_=
        for (int y = src_move.height() - 1; y != -1; --y) {
        for (int x = 0; x != src_move.width(); ++x) {
            dst_move(x, y) = src_move(x, y);
        }}
        auto whipe_row = make_sub_grid(src_move, VectorI(), k_rest_of_grid, k_tile_size);
        fill_with_checkerboard(whipe_row);
    }

    static cul::Rectangle<int> grass_bounds(int target_width, int target_height) {
        static const bool k_is_diamond = cut_f == cut_diamond;
        auto h = (target_height - k_tile_size) / 2;
        VectorI offset(0, k_has_grass_on_top ? 0 : h);
        h += k_tile_size;
        if (k_has_grass_on_top && k_is_diamond) {
            h += k_tile_size;
        }
        return cul::Rectangle<int>(offset, cul::Size2<int>(target_width, h));
    }

    static GrassMappings compute_grass_mappings
        (ConstSubGrid<Color> ground_target, ConstSubGrid<Color> grass_target)
    {
        verify_dims(ground_target);
        verify_dims(grass_target );
        GrassMappings rv;
        rv.reserve( ((ground_target.width () / k_tile_size) + 1)
                   *((ground_target.height() / k_tile_size) + 1) );
        for (int y = 0; y != ground_target.height(); y += k_tile_size) {
        for (int x = 0; x != ground_target.width (); x += k_tile_size) {
            auto subg = make_sub_grid(grass_target, VectorI(x, y), k_tile_size, k_tile_size);
            if (std::all_of(subg.begin(), subg.end(), is_transparency)) continue;
            rv.emplace_back( VectorI(x, y), VectorI(x, y) );
        }}
        return rv;
    }

    static constexpr const bool k_has_grass_on_top = !kt_as_hole;
    std::vector<std::tuple<VectorI, VectorI>> m_grass_mappings;
    std::default_random_engine m_rng { std::random_device()() };
};

template <void (*cut_f)(SubGrid<Color>, bool), bool kt_as_hole>
class SeriesSlopes final : public CutsSlopes<cut_f, kt_as_hole> {
    using MergeInfo = Slopes::MergeInfo;
    void do_cuts(SubGrid<Color> ground_target, SubGrid<Color> grass_target) override
        { CutsSlopes<cut_f, kt_as_hole>::do_cuts_task(ground_target, grass_target); }

    void wait_and_merge(SubGrid<Color>, MergeInfo &) override
        { /*Slopes::identify_tiles<cut_f>(info, target);*/ }

    std::unique_ptr<Slopes> clone() const override
        { return std::make_unique<SeriesSlopes>(*this); }
};

template <void (*cut_f)(SubGrid<Color>, bool), bool kt_as_hole>
class ParallelSlopes final : public CutsSlopes<cut_f, kt_as_hole> {
public:
    ParallelSlopes() {}
    ParallelSlopes(const ParallelSlopes &): m_future() {}
    ParallelSlopes(ParallelSlopes &&) = default;

    ParallelSlopes & operator = (const ParallelSlopes &) { return *this; }
    ParallelSlopes & operator = (ParallelSlopes &&) = default;

    ~ParallelSlopes() final {}

private:
    using BaseClass = CutsSlopes<cut_f, kt_as_hole>;
    using MergeInfo = Slopes::MergeInfo;

    void do_cuts(SubGrid<Color> ground_target, SubGrid<Color> grass_target) override {
        m_future = std::make_shared<std::future<void>>( std::async(std::launch::async, [=]() {
            BaseClass::do_cuts_task(ground_target, grass_target);
        }) );
    }

    void wait_and_merge(SubGrid<Color>, MergeInfo &) override {
        m_future->wait();
        m_future->get();
    }

    std::unique_ptr<Slopes> clone() const override
        { return std::make_unique<ParallelSlopes>(*this); }

    std::shared_ptr<std::future<void>> m_future;
};

VectorI get_position(const Slopes::Task & el) { return std::get<1>(el); }
int get_target_width(const Slopes::Task & el) { return std::get<2>(el); }
int get_target_height(const Slopes::Task & el) { return std::get<3>(el); }
const Slopes * get_slopes(const Slopes::Task & el) { return std::get<0>(el).get(); }
Slopes * get_slopes(Slopes::Task & el) {
    const auto & const_el = el;
    return const_cast<Slopes *>(get_slopes(const_el));
}

SubGrid<Color> make_sub_grid(SubGrid<Color> target, const Slopes::Task & task) {
    return make_sub_grid(target, get_position(task), get_target_width(task), get_target_height(task));
}

template <void (*cut_f)(SubGrid<Color>, bool), bool kt_as_hole>
/* static */ std::tuple<std::unique_ptr<Slopes>, VectorI, int, int> Slopes::make_task
    (VectorI r, int width, int height)
{
    std::unique_ptr<Slopes> ptr = nullptr;
    if constexpr (k_use_mutlithreading) {
        ptr = std::make_unique<ParallelSlopes<cut_f, kt_as_hole>>();
    } else {
        ptr = std::make_unique<SeriesSlopes<cut_f, kt_as_hole>>();
    }

    return std::make_tuple(
        std::move(ptr),
        r*k_tile_size,
        width*k_tile_size, height*k_tile_size );
}

template <void (*cut_f)(SubGrid<Color>, bool)>
/* static */ View<const Slopes::Task *> Slopes::get_tasks_for() {
    // doubled for four way symetric
    // + 1 tile height for grass and shading on circles
    static const std::array k_circle_tasks = {
        make_task<cut_circle, false>(VectorI( 0,  0), 5*2, 5*2 + 1),
        make_task<cut_circle, true >(VectorI(10,  0), 5*2, 5*2 + 1),
        make_task<cut_circle, false>(VectorI( 0, 11), 7*2, 7*2 + 1),
        make_task<cut_circle, true >(VectorI(14, 11), 7*2, 7*2 + 1),
    };
    // + 2 tile height for diamonds
    static const std::array k_diamond_tasks = {
        make_task<cut_diamond, false>(VectorI( 0, 0), 1*2, 1*2 + 2),
        make_task<cut_diamond, false>(VectorI( 2, 0), 2*2, 1*2 + 2),
        make_task<cut_diamond, false>(VectorI( 6, 0), 3*2, 1*2 + 2),
        make_task<cut_diamond, false>(VectorI(12, 0), 4*2, 1*2 + 2),

        make_task<cut_diamond, false>(VectorI( 0, 4), 3*2, 2*2 + 2),
        make_task<cut_diamond, false>(VectorI( 6, 4), 2*2, 3*2 + 2),

        make_task<cut_diamond, false>(VectorI(10, 4), 1*2, 2*2 + 2),
        make_task<cut_diamond, false>(VectorI(12, 4), 1*2, 3*2 + 2),
        make_task<cut_diamond, false>(VectorI(14, 4), 1*2, 4*2 + 2),
    };
    static const Task * k_circle_beg = k_circle_tasks.data();
    static const Task * k_circle_end = k_circle_beg + k_circle_tasks.size();
    static const Task * k_diamond_beg = k_diamond_tasks.data();
    static const Task * k_diamond_end = k_diamond_beg + k_diamond_tasks.size();
    if (cut_f == cut_circle) {
        return View(k_circle_beg, k_circle_end);
    } else if (cut_f == cut_diamond) {
        return View(k_diamond_beg, k_diamond_end);
    }
    throw BadBranchException();
}

std::tuple<Grid<Color>, Slopes::MergeInfo, Grid<Color>> do_grid_tasks(View<const Slopes::Task *> tasks_view) {
    int twidth = 0, theight = 0;
    for (const auto & task : tasks_view) {
        twidth  = std::max( twidth , get_position(task).x + get_target_width (task) );
        theight = std::max( theight, get_position(task).y + get_target_height(task) );
    }
    std::vector<Slopes::Task> tasks;
    for (const auto & task : tasks_view) {
        tasks.emplace_back( Slopes::copy_task(task) );
    }

    Grid<Color> target;
    Grid<Color> grass_target;
    target      .set_size(twidth, theight, k_transparency);
    grass_target.set_size(twidth, theight, k_transparency);
    for (auto & task : tasks) {
        get_slopes(task)->do_cuts(make_sub_grid(target, task), make_sub_grid(grass_target, task));
    }
    Slopes::MergeInfo info;
    for (auto & task : tasks) {
        get_slopes(task)->wait_and_merge(make_sub_grid(target, task), info);
    }
    return std::make_tuple(std::move(target), std::move(info), std::move(grass_target));
}

static constexpr const int k_water_fall_height = 13*k_tile_size;
static constexpr const int k_water_fall_width  = 5*k_tile_size;

void gen_waterfalls(SubGrid<Color> rv) {
    auto waterfall_list = {
        gen_waterfall(k_tile_size, k_water_fall_height, 0, 0),
        gen_waterfall(k_tile_size, k_water_fall_height, 1, 0),
        gen_waterfall(k_tile_size, k_water_fall_height, 2, 0),
        gen_waterfall(k_tile_size, k_water_fall_height, 3, 0)
    };
    VectorI start;// = VectorI(k_tile_size, 0);// 112, 368);
    for (const auto & waterfall : waterfall_list) {
        auto subg = make_sub_grid(rv, start, waterfall.width(), waterfall.height());
        for (VectorI r; r != subg.end_position(); r = subg.next(r)) {
            subg(r) = waterfall(r);
        }
        start += VectorI(k_tile_size, 0);
    }

    static constexpr const auto k_magic_tile_gfx = ""
        // 0123456789ABCDEF
        """XXXXXXXXXXXXXXXX" // 0
        """X              X" // 1
        """X              X" // 2
        """X W    W    W  X" // 3
        """X W    W    W  X" // 4
        """X W    W    W  X" // 5
        """X W    W    W  X" // 6
        """X  W   W   W   X" // 7
        """X  W   W   W   X" // 8
        """X   W W W W    X" // 9
        """X   W W W W    X" // A
        """X   W W W W    X" // B
        """X    W   W     X" // C
        """X              X" // D
        """X              X" // E
        """XXXXXXXXXXXXXXXX" // F
        ;
    auto magic_tile_subg = make_sub_grid(rv, start, k_tile_size, k_tile_size);
    for (VectorI r; r != magic_tile_subg.end_position(); r = magic_tile_subg.next(r)) {
        magic_tile_subg(r) = [r] {
            auto idx = r.x + r.y*k_tile_size;
            assert(idx < int(strlen(k_magic_tile_gfx)));
            switch (k_magic_tile_gfx[idx]) {
            case ' ': return Color::Black;
            case 'W': return Color(180, 180, 255);
            case 'X': return Color(255, 100, 100);
            default: throw std::runtime_error("");
            }
        } ();
    }
}

} // end of <anonymous> namespace

static void generate_platform_texture(SubGrid<Color> target_grid);

template <void (*cut_f)(SubGrid<Color>, bool), bool as_hole>
static void gen_slopes(SubGrid<Color> target, VectorI offset, int width, int height);

static void copy_subgrid_to(ConstSubGrid<Color> src, SubGrid<Color> dest) {
    if (src.width() != dest.width() || src.height() != dest.height()) {
        throw std::out_of_range("");
    }
    std::copy(src.begin(), src.end(), dest.begin());
}
#if 0
static void copy_subgrid_to
    (ConstSubGrid<Color> src, SubGrid<Color> dest,
     VectorI src_pos, VectorI dest_pos, int width, int height)
{
    copy_subgrid_to(make_const_sub_grid(src, src_pos, width, height));
}
#endif
Grid<Color> generate_atlas_2() {
    static const auto k_circle_tasks  = Slopes::get_tasks_for<cut_circle >();
    static const auto k_diamond_tasks = Slopes::get_tasks_for<cut_diamond>();

    auto circle_slopes  = do_grid_tasks(k_circle_tasks );
    auto diamond_slopes = do_grid_tasks(k_diamond_tasks);

    to_image(std::get<0>(diamond_slopes)).saveToFile("/media/ramdisk/diamonds.png");
    to_image(std::get<0>(circle_slopes)).saveToFile("/media/ramdisk/circles.png");
    std::default_random_engine rng { std::random_device()() };

    to_image(std::get<2>(diamond_slopes)).saveToFile("/media/ramdisk/diamondgrass.png");
    to_image(std::get<2>(circle_slopes)).saveToFile("/media/ramdisk/circlegrass.png");

    auto flats   = gen_flats(rng);
    auto indents = gen_all_indents();
    to_image(flats).saveToFile("/media/ramdisk/flats.png");


    using std::get;
    Grid<Color> composite;
    composite.set_size(
        std::max( { get<0>(diamond_slopes).width(), get<0>(circle_slopes).width() } ),
          get<0>(diamond_slopes).height() + get<0>(circle_slopes).height()
        + std::max(flats.height(), indents.height()),
        k_transparency);

    return Grid<Color>();
}

/* free fn */ Grid<Color> generate_atlas() {
    const auto seed = std::random_device()();
    Grid<Color> rv;
    {
    using std::make_pair;
    get_base_checkerboard();

    using ProdFunc = void (*)(SubGrid<Color>, bool);
    auto gen_slopes_ = [&seed](int width, int height, ProdFunc pf, bool as_hole) {
        std::default_random_engine rng { seed };
        static constexpr const int k_rest_of = k_rest_of_grid;

        auto g = get_base_checkerboard();
        g = extend(g, width*2,
                   height*2 + (as_hole && pf == cut_circle ? 0 : 1) + (as_hole ? 1 : 0));
        pf        (SubGrid<Color>(g, k_rest_of, height*2*k_tile_size), as_hole);

        add_border(SubGrid<Color>(g, k_rest_of, height*2*k_tile_size));

        if (pf == cut_diamond || (!as_hole && pf == cut_circle))
            g = push_down(g, as_hole ? height*k_tile_size : 0, k_tile_size);

        SubGrid<Color> grass_seg;
        if (as_hole) {
            grass_seg = SubGrid<Color>(g, VectorI(0, height*k_tile_size), k_rest_of, k_rest_of);
        } else {
            grass_seg = SubGrid<Color>(g, k_rest_of, (height + 1)*k_tile_size);
        }
#       if 0
        add_grass(grass_seg, rng);
#       endif
        return g;
    };
#   if 0
    auto bottom_half = [&gen_slopes_](int width, int height, ProdFunc pf, bool as_hole) {
        // div 16, round down
        auto g = gen_slopes_(width, height, pf, as_hole);
        Grid<Color> dest;
        auto h = g.height() / 2 - ((g.height() / 2) % 16 ? 8 : 0);
        dest.set_size(g.width(), h);
        for (VectorI r; r != dest.end_position(); r = dest.next(r)) {
            dest(r) = g(r + VectorI(0, g.height() - h));
        }
        return dest;
    };

    auto top_half = [&gen_slopes_](int width, int height, ProdFunc pf, bool as_hole) {
        // div 16, round up
        auto g = gen_slopes_(width, height, pf, as_hole);
        auto h = g.height() / 2 + ((g.height() / 2) % 16 ? 8 : 0);
        g.set_size(g.width(), h);
        return g;
    };
#   endif

    using std::bind;
    using std::move;
    using Rng = std::default_random_engine;

    std::vector<std::pair<std::function<Grid<Color>()>, VectorI>>
        task_list
    {
        make_pair(bind(gen_slopes_,  7,  7, cut_circle, false), VectorI( 48,  32) + VectorI(3, -2)*k_tile_size),
        make_pair(bind(gen_slopes_,  7,  7, cut_circle, true ), VectorI(320,   0)),
        make_pair(bind(gen_slopes_,  5,  5, cut_circle, false), VectorI(352,  16)),
        make_pair(bind(gen_slopes_,  5,  5, cut_circle, true ), VectorI(320, 240)),
        make_pair(bind(gen_flats      ,Rng { seed }), VectorI(544,  64)),
        make_pair(     gen_all_indents              , VectorI(592,  64)),

        make_pair(bind(gen_slopes_, 1, 1, cut_diamond, false), VectorI(576, 288)),
        make_pair(bind(gen_slopes_, 1, 2, cut_diamond, false), VectorI(608, 224)),
        make_pair(bind(gen_slopes_, 1, 3, cut_diamond, false), VectorI(544, 288)),
        make_pair(bind(gen_slopes_, 1, 4, cut_diamond, false), VectorI(640, 128)),
        make_pair(bind(gen_slopes_, 2, 1, cut_diamond, false), VectorI(544, 224)),
        make_pair(bind(gen_slopes_, 3, 1, cut_diamond, false), VectorI(576, 352)),
        make_pair(bind(gen_slopes_, 4, 1, cut_diamond, false), VectorI(544,   0)),
        make_pair(bind(gen_slopes_, 3, 2, cut_diamond, false), VectorI(544, 128)),
        make_pair(bind(gen_slopes_, 2, 3, cut_diamond, false), VectorI(480, 240)),
    };
    using FutureGrid = std::future<Grid<Color>>;
    std::vector<std::pair<FutureGrid, VectorI>> futures;
    futures.reserve(task_list.size());

    std::vector<std::thread> threads;
    threads.reserve(task_list.size());
    for (auto & [task, loc] : task_list) {
        std::packaged_task<Grid<Color>()> pkgtask { std::move(task) };
        futures.emplace_back(make_pair(FutureGrid { pkgtask.get_future() }, loc));
        threads.emplace_back(std::move(pkgtask));
        threads.back().detach();
    }

    for (auto & [future, _] : futures) {
        (void)_;
        future.wait();
    }

    std::vector<std::pair<Grid<Color>, VectorI>> products;
    products.reserve(futures.size());
    for (auto & [future, loc] : futures) {
        try {
            products.emplace_back(future.get(), loc);
        } catch (std::exception & exp) {
            std::cerr << exp.what() << std::endl;
        }
    }

    int w = 0, h = 0;
    for (const auto & [grid, r] : products) {
        w = std::max(w, r.x + grid.width ());
        h = std::max(h, r.y + grid.height());
    }
    Grid<Color> composite;
    composite.set_size(w, h, k_transparency);
    for (const auto & [grid, r] : products) {
        for (VectorI u; u != grid.end_position(); u = grid.next(u)) {
            if (grid(u).a == 0) continue;
            composite(r + u) = grid(u);
        }
    }

    generate_platform_texture(make_sub_grid(composite, VectorI(0, k_tile_size*14), k_tile_size*3, k_tile_size*4));

    assert(composite.height() % k_tile_size == 0 && composite.width() % k_tile_size == 0);
    static const VectorI k_chosen_checkerboard = VectorI();//34, 8)*16;
    std::copy(get_base_checkerboard().begin(), get_base_checkerboard().end(),
              make_sub_grid(composite, VectorI(), k_tile_size, k_tile_size).begin());
    static auto x_out_tile = [](SubGrid<Color> subg) {
        assert(subg.width() == k_tile_size && subg.height() == k_tile_size);
        static constexpr const auto k_x_out_gfx =
            // 0123456789ABCDEF
            """XX            XX" // 0
            """X              X" // 1
            """  Xx         Xx " // 2
            """   Xx       Xx  " // 3
            """    Xx     Xx   " // 4
            """     Xx   Xx    " // 5
            """      XxxXx     " // 6
            """       XXx      " // 7
            """       XXx      " // 8
            """      XxxXx     " // 9
            """     Xx   Xx    " // A
            """    Xx     Xx   " // B
            """   Xx       Xx  " // C
            """  Xx         X  " // D
            """X              X" // E
            """XX            XX" // F
            ;
        for (VectorI r; r != subg.end_position(); r = subg.next(r)) {
            subg(r) = [] (std::size_t idx) {
                switch (k_x_out_gfx[idx]) {
                case ' ': return k_transparency;
                case 'x': return Color(60, 60, 60, 255);
                case 'X': return Color::Black;
                }
                throw BadBranchException();
            } (std::size_t(r.x + r.y*k_tile_size));

        }
    };

    std::vector<VectorI> empty_squares =
            [](ConstSubGrid<Color> composite) {
    std::vector<VectorI> empty_squares;
    for (int y = 0; y < composite.height(); y += k_tile_size) {
    for (int x = 0; x < composite.width (); x += k_tile_size) {
        // delete all but one base checkboard
        if (k_chosen_checkerboard == VectorI(x, y)) continue;

        auto subg = make_const_sub_grid/*ConstSubGrid<Color> subg*/(composite, VectorI(x, y), k_tile_size, k_tile_size);
#       if 0
        bool is_a_checkboard = [](ConstSubGrid<Color> subg) {
            const auto & base = get_base_checkerboard();
            for (VectorI r; r != subg.end_position(); r = subg.next(r)) {
                if (base(r) == subg(r)) continue;
                return false;
            }
            return true;
        } (subg);
#       endif

        bool is_empty_tile = [](ConstSubGrid<Color> subg) {
            for (auto color : subg) {
                if (color.a != 0) return false;
            }
            return true;
        } (subg);

        if (/*is_a_checkboard ||*/ is_empty_tile) {
            empty_squares.emplace_back(x, y);
#           if 0
            x_out_tile(subg);
#           endif
        }
    }}
    return empty_squares;
    } (composite);
    rv.swap(composite);
    }

    gen_waterfalls(make_sub_grid(rv, VectorI(k_tile_size, 0), k_water_fall_width, k_water_fall_height));
#   if 0
    static constexpr const int k_water_fall_height = 13*k_tile_size;
    static constexpr const int k_water_fall_width  = 5*k_tile_size;
    [](VectorI start, SubGrid<Color> rv){


    auto waterfall_list = {
        gen_waterfall(k_tile_size, k_water_fall_height, 0, 0),
        gen_waterfall(k_tile_size, k_water_fall_height, 1, 0),
        gen_waterfall(k_tile_size, k_water_fall_height, 2, 0),
        gen_waterfall(k_tile_size, k_water_fall_height, 3, 0)
    };
    //VectorI start = VectorI(k_tile_size, 0);// 112, 368);
    for (const auto & waterfall : waterfall_list) {
        auto subg = make_sub_grid(rv, start, waterfall.width(), waterfall.height());
        for (VectorI r; r != subg.end_position(); r = subg.next(r)) {
            subg(r) = waterfall(r);
        }
        start += VectorI(k_tile_size, 0);
    }

    static constexpr const auto k_magic_tile_gfx = ""
        // 0123456789ABCDEF
        """XXXXXXXXXXXXXXXX" // 0
        """X              X" // 1
        """X              X" // 2
        """X W    W    W  X" // 3
        """X W    W    W  X" // 4
        """X W    W    W  X" // 5
        """X W    W    W  X" // 6
        """X  W   W   W   X" // 7
        """X  W   W   W   X" // 8
        """X   W W W W    X" // 9
        """X   W W W W    X" // A
        """X   W W W W    X" // B
        """X    W   W     X" // C
        """X              X" // D
        """X              X" // E
        """XXXXXXXXXXXXXXXX" // F
        ;
    auto magic_tile_subg = make_sub_grid(rv, start, k_tile_size, k_tile_size);
    for (VectorI r; r != magic_tile_subg.end_position(); r = magic_tile_subg.next(r)) {
        magic_tile_subg(r) = [r] {
            auto idx = r.x + r.y*k_tile_size;
            assert(idx < int(strlen(k_magic_tile_gfx)));
            switch (k_magic_tile_gfx[idx]) {
            case ' ': return Color::Black;
            case 'W': return Color(180, 180, 255);
            case 'X': return Color(255, 100, 100);
            default: throw std::runtime_error("");
            }
        } ();
    }
    }(VectorI(k_tile_size, 0), rv);
#   endif

    return rv;
}

static void gen_soft_cieling(SubGrid<Color> target_grid) {
    static const auto & k_base = get_base_checkerboard();
    static constexpr const auto k_cut_all = -1;
    static auto get_cut_amount = [] (int depth) {
        static constexpr const auto k_cuts =
            // 01234567
            """xxxxxxxx" // 0
            """xxxxxxxx" // 1
            """xxxxxxxx" // 2
            """xxxxxxxx" // 3
            """xxxxxxxx" // 4
            """xxxxxxxx" // 5
            """ xxxxxxx" // 6
            """ xxxxxxx" // 7
            """  xxxxxx" // 8
            """    xxxx" // 9
            """      xx" // A
            """       x" // B
            """        " // C
            """        " // D
            """        " // E
            """        " /* F */;
            assert(depth <= 0xF);
            auto start = k_cuts + depth*8;
            auto end   = start + 8;
            for (auto itr = start; itr != end; ++itr) {
                if (*itr == 'x') return int(itr - start);
            }
            return k_cut_all;
    };
    int last_tile_end = target_grid.width() - (target_grid.width() % k_tile_size);
    assert(last_tile_end <= target_grid.width());
    for (int x = 0; x != last_tile_end; x += k_tile_size) {
        auto subg = make_sub_grid(target_grid, VectorI(x, 0), k_tile_size, k_tile_size);
        assert(subg.width() == k_tile_size && subg.height() == k_tile_size);
        std::copy(k_base.begin(), k_base.end(), subg.begin());
    }
    for (int y = 0            ; y != k_tile_size        ; ++y) {
    for (int x = last_tile_end; x != target_grid.width(); ++x) {
        target_grid(x, y) = k_base(x - last_tile_end, y);
    }}
    for (int y = 0; y != k_tile_size; ++y) {
        int cut_this_line = get_cut_amount(y);
        if (cut_this_line == k_cut_all) {
            for (int x = 0; x != target_grid.width(); ++x) {
                target_grid(x, y) = k_transparency;
            }
        } else {
            for (int x = 0; x != target_grid.width(); ++x) {
                if (x < cut_this_line || x > (target_grid.width() - cut_this_line))
                    { target_grid(x, y) = k_transparency; }
            }
        }
    }
}

static void generate_platform_texture(SubGrid<Color> target_grid) {
    assert(target_grid.width() >= k_tile_size*2);

    static constexpr const auto k_plat_y = k_tile_size*3;
    gen_soft_cieling(make_sub_grid(target_grid, VectorI(0, k_plat_y), k_tile_size, k_rest_of_grid));
    std::default_random_engine rng { std::random_device()() };
    add_grass(make_sub_grid(target_grid, k_rest_of_grid, k_tile_size*2),
              make_const_sub_grid(target_grid, VectorI(0, k_tile_size*2), k_rest_of_grid, k_rest_of_grid),
              rng);
    add_grass(make_sub_grid(target_grid, VectorI(0, k_tile_size), k_rest_of_grid, k_rest_of_grid), rng);
}

/* free fn */ Grid<Color> generate_platform_texture(int inner_width) {
    Grid<Color> rv;
    rv.set_size(k_tile_size*2 + inner_width, k_tile_size*4, k_transparency);
    generate_platform_texture(rv);
    return rv;
}

/* free fn */ sf::Image to_image(const Grid<Color> & grid) {
    sf::Image img;
    img.create(unsigned(grid.width()), unsigned(grid.height()));
    for (VectorI r; r != grid.end_position(); r = grid.next(r)) {
        img.setPixel(unsigned(r.x), unsigned(r.y), grid(r));
    }
    return img;
}

namespace {

inline Color mk_color(int hex)
    { return Color(hex >> 16, (hex >> 8) & 0xFF, hex & 0xFF); }

template <typename T>
inline T get(std::initializer_list<T> list, int x)
    { return *(list.begin() + x); }

std::array<VectorI, 4> get_mirror_points(ConstSubGrid<Color>, VectorI);
std::array<VectorI, 4> get_mirror_points(ConstSubGrid<Color>, int x, int y);

void darken_color(Color &, int index);

void do_indent(SubGrid<Color>, bool is_corner, bool inverse);

void rotate_90(SubGrid<Color>);

void swap_grid_contents(SubGrid<Color>, SubGrid<Color>);

const Grid<Color> & get_base_checkerboard() {
    static Grid<Color> rv;
    if (!rv.is_empty()) return rv;

    constexpr const int k_width  = k_tile_size;
    constexpr const int k_height = k_tile_size;
    constexpr const int k_dark   = 2;
    constexpr const int k_light  = 0;
    static auto on_edge = [](VectorI r) { return ((r.x + 1) % 8 == 0) || ((r.y + 1) % 8 == 0); };
    Grid<int> tile;
    tile.set_size(k_width, k_height);
    for (VectorI r; r != tile.end_position(); r = tile.next(r)) {
        int c;
        if ((r.x < 8 && r.y < 8) || (r.x >= 8 && r.y >= 8)) {
            c = k_light;
        } else {
            c = k_dark;
        }
        if (on_edge(r)) { ++c; }
        tile(r) = c;
    }
    static const auto k_colors = {
        mk_color(0xA8760A),
        mk_color(0xB0800B),
        mk_color(0x704008),
        mk_color(0x906009)
    };
    rv.set_size(tile.width(), tile.height());
    for (VectorI r; r != tile.end_position(); r = tile.next(r)) {
        rv(r) = get(k_colors, tile(r));
    }
    return rv;
}

// assumes four way symetric
void add_border(SubGrid<Color> grid) {
    using std::make_pair;
    static const Color k_light = mk_color(0x906000);
    static const Color k_dark  = mk_color(0x603000);
#   if 0
    static const Color k_trnsp(0, 0, 0, 0);
#   endif
    auto safe_write = [&grid](VectorI r) -> Color & {
        static Color badref;
        if (grid.has_position(r)) return std::ref(grid(r));
        return std::ref(badref);
    };

    const int last_x = grid.width () - 1;
    const int last_y = grid.height() - 1;

    for (int y = 0; y != grid.height() / 2; ++y) {
    for (int x = 0; x != grid.width () / 2; ++x) {
        VectorI v(x, y);

        VectorI r(x + 1, y);
        VectorI l(x - 1, y);
        int x_ways = 0;
        if (grid.has_position(r)) {
            if (grid(r) == k_transparency && grid(v) != k_transparency) {
                x_ways = -1;
            }
        }
        if (grid.has_position(l)) {
            if (grid(l) == k_transparency && grid(v) != k_transparency) {
                // we can't go both ways
                x_ways = x_ways == 0 ? 1 : 0;
            }
        }
        if (x_ways != 0) {
            auto dark_list = {
                VectorI(v.x + x_ways         ,          v.y),
                VectorI(v.x + x_ways         , last_y - v.y),
                VectorI(last_x - v.x - x_ways,          v.y),
                VectorI(last_x - v.x - x_ways, last_y - v.y),
            };
            for (auto pt : dark_list)
                { safe_write(pt) = k_dark; }
        }

        int y_ways = 0;
        VectorI t(x, y - 1);
        VectorI b(x, y + 1);
        if (grid.has_position(t)) {
            if (grid(t) == k_transparency && grid(v) != k_transparency) {
                y_ways = 1;
            }
        }
        if (grid.has_position(b)) {
            if (grid(b) == k_transparency && grid(v) != k_transparency) {
                y_ways = y_ways == 0 ? -1 : 0;
            }
        }
        if (y_ways != 0) {
            auto dark_list = {
                VectorI(v.x         ,          v.y + y_ways),
                VectorI(v.x         , last_y - v.y - y_ways),
                VectorI(last_x - v.x,          v.y + y_ways),
                VectorI(last_x - v.x, last_y - v.y - y_ways),
            };
            for (auto pt : dark_list)
                { safe_write(pt) = k_dark; }
        }

        if (y_ways != 0 || x_ways != 0) {
            for (const auto & pt : get_mirror_points(grid, x, y))
                { grid(pt) = k_light; }
        }
    }}
}

void cut_circle(SubGrid<Color> grid, bool as_hole) {
    auto radius = std::min(grid.width(), grid.height()) / 2;
    VectorI center(grid.width() / 2, grid.height() / 2);
    for (int y = 0; y != grid.height() / 2; ++y) {
    for (int x = 0; x != grid.width () / 2; ++x) {
        auto diff = center - VectorI(x, y);
        bool is_inside = (diff.x*diff.x + diff.y*diff.y) < radius*radius;
        if ((is_inside && as_hole) || (!is_inside && !as_hole)) {
            for (const auto & pt : get_mirror_points(grid, x, y))
                { grid(pt) = k_transparency; }
        }
    }}
#   if 0
    auto bump_color = as_hole ? Color(0, 0, 0, 0) : Color::White;

    grid(grid.width() / 2    , grid.height() - 1) = bump_color;
    grid(grid.width() / 2 + 1, grid.height() - 1) = bump_color;

    grid(grid.width() / 2    , 0) = bump_color;
    grid(grid.width() / 2 + 1, 0) = bump_color;

    grid(0, grid.height() / 2    ) = bump_color;
    grid(0, grid.height() / 2 - 1) = bump_color;

    grid(grid.width() - 1, grid.height() / 2    ) = bump_color;
    grid(grid.width() - 1, grid.height() / 2 - 1) = bump_color;
#   endif
    std::vector<VectorI> edge_pixels;
    auto one_neighbor_is = [grid](VectorI r, bool (*f)(Color)) {
        auto neighbors = {
            VectorI(1, 0), VectorI(0, 1), VectorI(-1, 0), VectorI(0, -1),
            VectorI(1, 1), VectorI(-1, 1), VectorI(1, -1), VectorI(-1, -1)
        };
        for (auto o : neighbors) {
            auto n = o + r;
            if (!grid.has_position(n)) continue;
            if (f(grid(n))) return true;
        }
        return false;
    };
    for (int y = 0; y != grid.height(); ++y) {
    for (int x = 0; x != grid.width (); ++x) {
        if (!(x % 4 == 0 || y % k_tile_size == 0 || y % k_tile_size == 15)) continue;
        static const Color k_t = k_transparency;
        VectorI r(x, y);
        auto transparent = [](Color c){ return c == k_t; };
        auto opaque      = [](Color c){ return c != k_t; };
        if (opaque(grid(r)) && one_neighbor_is(r, transparent) && one_neighbor_is(r, opaque)) {
            edge_pixels.push_back(r);
        }
    }}
    // assumption, points are linked in a "ring"
    edge_pixels.push_back(edge_pixels.front());
#   if 0
    segments.clear();
    segments.reserve(edge_pixels.size() - 1);
    while (true) {
        auto b = edge_pixels.back();
        edge_pixels.pop_back();
        if (edge_pixels.empty()) break;

        static const VectorI k_uninit(-1, -1);
        VectorI closest = k_uninit;
        for (const auto & other : edge_pixels) {
            auto magsqr = [](VectorI r) { return r.x*r.x + r.y*r.y; };
            auto closest_diff = b - closest;
            auto other_diff   = b - other  ;
            if (closest == k_uninit) {
                closest = other;
            } else if (magsqr(closest_diff) > magsqr(other_diff)) {
                closest = other;
            }
        }
        assert(b       != k_uninit);
        assert(closest != k_uninit);
        LineSegment seg {VectorD(b), VectorD(closest)};
        segments.push_back(seg);
    }
#   endif
}

template <typename T>
Grid<T> extend(const Grid<T> & gin, int wmul, int hmul) {
    Grid<T> gout;
    gout.set_size(gin.width()*wmul, gin.height()*hmul);
    for (VectorI r; r != gout.end_position(); r = gout.next(r)) {
        gout(r) = gin(r.x % gin.width(), r.y % gin.height());
    }
    return gout;
}

void cut_diamond(SubGrid<Color> grid, bool as_hole) {
    assert(grid.width() % 2 == 0 && grid.height() % 2 == 0);
    auto is_inside_f = [&grid](VectorI r) {
        r.x = magnitude(r.x);
        r.y = magnitude(r.y);
        auto y_max = grid.height() / 2;
        auto x_max = grid.width () / 2;
        auto y_val = y_max - (y_max*r.x) / x_max;
        return r.y <= y_val;
    };
    VectorI center(grid.width() / 2, grid.height() / 2);
    for (int y = 0; y != grid.height() / 2; ++y) {
    for (int x = 0; x != grid.width () / 2; ++x) {
        auto adj = VectorI(x, y) - center;
        bool is_inside = is_inside_f(adj);
        if ((is_inside && as_hole) || (!is_inside && !as_hole)) {
            for (const auto & pt : get_mirror_points(grid, x, y))
                { grid(pt) = k_transparency; }
        }
    }}
}

Grid<Color> push_down(ConstSubGrid<Color> grid, int x_line, int y_amount) {
    Grid<Color> rv;
    rv.set_size(grid.width(), grid.height());
    SubGrid<Color> head(rv                               , k_rest_of_grid, x_line        );
    SubGrid<Color> body(rv, VectorI(0, x_line           ), k_rest_of_grid, y_amount      );
    SubGrid<Color> foot(rv, VectorI(0, x_line + y_amount), k_rest_of_grid, k_rest_of_grid);
    for (VectorI r; r != head.end_position(); r = head.next(r)) {
        head(r) = grid(r);
    }
    for (VectorI r; r != body.end_position(); r = body.next(r)) {
        body(r) = k_transparency;
    }

    for (VectorI r; r != foot.end_position(); r = foot.next(r)) {
        foot(r) = grid(r + VectorI(0, x_line));
    }
    return rv;
}


template <bool kt_ground_is_const>
void add_grass_impl
    (SubGrid<Color> grass_dest,
     std::conditional_t<kt_ground_is_const, ConstSubGrid<Color>, SubGrid<Color>> ground,
     std::default_random_engine & rng)
{
    // dest and source, maybe the same! (and that's ok!)
    // but they must be the same size
    assert(grass_dest.width() == ground.width() && grass_dest.height() == ground.height());
    using IntDistri = std::uniform_int_distribution<int>;
    static const auto k_greens = {
        mk_color(0x008000),
        mk_color(0x00A000),
        mk_color(0x00C000),
        mk_color(0x20D820),
    };
    for (VectorI r; r != grass_dest.end_position(); r = grass_dest.next(r)) {
        VectorI upper = r + VectorI(0, -1);
        if (!grass_dest.has_position(upper)) continue;
        if (   ground(upper) == k_transparency
            && ground(upper) != ground(r))
        {
            // at least three green, two below, one  above
            // at most  six   green, ''       , four above
            int over_amount =  IntDistri(1, 7)(rng);
            int deep_over   =  IntDistri(0, 7)(rng);
            int mid_over    =  IntDistri(0, 3)(rng);
            int grass_start = -IntDistri(2, 4)(rng);
            int offset = static_cast<int>((r.x / 4) % 2 == 0);
            // rest are light
            // starts at the shadow
            int yd = (over_amount > 3 ? -4 : -3) + grass_start;
            for (; yd != over_amount; ++yd) {
                if (!grass_dest.has_position(r - VectorI(0, yd))) continue;
                auto t = r - VectorI(0, yd);

                if (yd < grass_start) {
                    // do nothing
                    if constexpr (!kt_ground_is_const) darken_color(ground(t) , 2);
                } else if (yd <= 0) {
                    grass_dest(t) = get(k_greens, 0 + offset);
                    if constexpr (!kt_ground_is_const) darken_color(ground(t) , 2);
                } else if (deep_over > 0) {
                    --deep_over;
                    grass_dest(t) = get(k_greens, 0 + offset);
                } else if(mid_over > 0) {
                    --mid_over;
                    grass_dest(t) = get(k_greens, 1 + offset);
                } else {
                    grass_dest(t) = get(k_greens, 2 + offset);
                }
            }
        }
    }
}

void add_grass
    (SubGrid<Color> grass_dest, SubGrid<Color> ground,
     std::default_random_engine & rng)
{
    add_grass_impl<false>(grass_dest, ground, rng);
}

void add_grass(SubGrid<Color> grass_dest, ConstSubGrid<Color> ground, std::default_random_engine & rng) {
    add_grass_impl<true>(grass_dest, ground, rng);
}

Grid<Color> gen_flats(std::default_random_engine & rng) {
    static constexpr const int k_rest_of = k_rest_of_grid;
    static constexpr const int k_size    = k_tile_size;
    const auto & base = get_base_checkerboard();
    Grid<Color> preimage;
    preimage.set_size(5*k_size, 5*k_size, k_transparency);
    for (int y = k_size; y != 4*k_size - (k_size / 2); ++y) {
    for (int x = k_size; x != 4*k_size; ++x) {
        preimage(x, y) = base(x % k_size, y % k_size);
    }}
    add_border(make_sub_grid(preimage, k_rest_of, preimage.height() - (k_size / 2)));
    //add_grass(make_sub_grid(preimage, k_rest_of, 4*k_size - (k_size / 2)), rng);

    SubGrid<Color> dark_tile(preimage, VectorI(2*k_size, 2*k_size), k_size, k_size);
    for (VectorI r; r != dark_tile.end_position(); r = dark_tile.next(r)) {
        darken_color(dark_tile(r), 3);
    }

    Grid<Color> rv;
    rv.set_size(3*k_size, 5*k_size);
    ConstSubGrid<Color> imgin(preimage, VectorI(k_size, 0), k_size*3, k_size*4);
    for (VectorI r; r != imgin.end_position(); r = imgin.next(r)) {
        rv(r) = imgin(r);
    }

    gen_soft_cieling(make_sub_grid(rv, VectorI(0, 4*k_size), k_rest_of, k_size));
    return rv;
}

Grid<Color> gen_all_indents() {
    auto grid = get_base_checkerboard();

    auto tile_width = grid.width();
    auto tile_height = grid.height();

    auto get_hexdecant = [&grid, tile_width, tile_height](int x, int y) {
        return SubGrid<Color>(grid, VectorI(x*tile_width, y*tile_height), tile_width, tile_height);
    };
    auto gen_set = [tile_width, tile_height](SubGrid<Color> grid) {
        auto ff = grid.make_sub_grid(tile_width, tile_height);
        do_indent(ff, false, false);

        auto ft = grid.make_sub_grid(VectorI(0, tile_height), tile_width, tile_height);
        do_indent(ft, false, true);

        auto tf = grid.make_sub_grid(VectorI(tile_width, 0), tile_width, tile_height);
        do_indent(tf, true, false);

        auto tt = grid.make_sub_grid(VectorI(tile_width, tile_height), tile_width, tile_height);
        do_indent(tt, true, true);
    };

    grid = extend(grid, 4, 4);
    auto quad_width = tile_width*2;
    auto quad_height = tile_height*2;
    {
    SubGrid<Color> g(grid, VectorI(), quad_width, quad_height);
    gen_set(g);
    }
    {
    SubGrid<Color> g(grid, VectorI(quad_width, 0), quad_width, quad_height);
    rotate_90(g);
    gen_set(g);
    rotate_90(g);
    rotate_90(g);
    rotate_90(g);
    }
    {
    SubGrid<Color> g(grid, VectorI(0, quad_height), quad_width, quad_height);
    rotate_90(g);
    rotate_90(g);
    gen_set(g);
    rotate_90(g);
    rotate_90(g);
    }
    {
    SubGrid<Color> g(grid, VectorI(quad_width, quad_height), quad_width, quad_height);
    rotate_90(g);
    rotate_90(g);
    rotate_90(g);
    gen_set(g);
    rotate_90(g);
    }
    swap_grid_contents(get_hexdecant(0, 0), get_hexdecant(1, 1));
    swap_grid_contents(get_hexdecant(3, 0), get_hexdecant(0, 1));
    swap_grid_contents(get_hexdecant(2, 3), get_hexdecant(1, 0));
    swap_grid_contents(get_hexdecant(0, 2), get_hexdecant(1, 1));

    swap_grid_contents(get_hexdecant(2, 0), get_hexdecant(2, 1));
    swap_grid_contents(get_hexdecant(0, 3), get_hexdecant(3, 1));
    swap_grid_contents(get_hexdecant(2, 3), get_hexdecant(2, 0));
    swap_grid_contents(get_hexdecant(3, 3), get_hexdecant(3, 0));

    swap_grid_contents(get_hexdecant(3, 3), get_hexdecant(2, 3));
    swap_grid_contents(get_hexdecant(0, 3), get_hexdecant(2, 3));

    swap_grid_contents(get_hexdecant(0, 2), get_hexdecant(0, 3));
    return grid;
}

Grid<Color> gen_waterfall
    (int w, int h, int pal_rotation_idx, int x_)
{
    //x_ = (x_ % 2) + 1;
    Grid<int> preimage;
    preimage.set_size(w, h);
    static constexpr const int k_max_colors = 4;
    [[maybe_unused]] static constexpr const int k_max_height = 16;

    int y_offset = 1;
    auto get_offset = [x_](int x) {
        x = (x + x_) % 8;
        return ((x > 3) ? 7 - x : x);
    };
    for (int x = 0; x != w; ++x) {
        int current_height = 1;
        int write_y = 0;
        int color_index = 0;

        for (int y = y_offset; y != h - y_offset; ++y) {
            if (preimage.has_position(x, y + y_offset)) {
                preimage(x, y + y_offset) = color_index;
            }
            if (++write_y >= current_height) {// ++write_y ==
                ++current_height ;//= std::min(k_max_height, current_height + 1);
                write_y = 0;
                if (--color_index == -1) color_index = k_max_colors - 1;
            }
        }
        y_offset = get_offset(x + 1);
    }

    static const auto k_palette = {
        mk_color(0x6060E0),
        mk_color(0x202090),
        mk_color(0x2020B0),
        mk_color(0x4040D0),
    };
    Grid<Color> image;
    image.set_size(w, h);
    for (VectorI r; r != image.end_position(); r = image.next(r)) {        
        image(r) = get(k_palette, (preimage(r) + pal_rotation_idx) % k_max_colors);
        // "eye razor" problem: I'll need to sync the lines with the frame
        // changes, and translations
        int intv = pal_rotation_idx % 2;
        if (r.x % 2 == intv) image(r).a = 100;
    }
    return image;
}

// ----------------------------- Helpers Level 1 ------------------------------

std::array<VectorI, 4> get_mirror_points(ConstSubGrid<Color> grid, VectorI r) {
    return {
        r,
        VectorI(grid.width() - r.x - 1,                 r.y    ),
        VectorI(               r.x    , grid.height() - r.y - 1),
        VectorI(grid.width() - r.x - 1, grid.height() - r.y - 1)
    };
}

std::array<VectorI, 4> get_mirror_points(ConstSubGrid<Color> grid, int x, int y)
    { return get_mirror_points(grid, VectorI(x, y)); }

void darken_color(Color & c, int index) {
    if (c == k_transparency) return;
    static auto darken_comp = [](uint8_t & c, int mul) {
        if (mul == 0) return;
        int res = c - 30 - (mul - 1)*20;
        c = uint8_t(std::max(0, res));
    };
    darken_comp(c.r, index);
    darken_comp(c.g, index);
    darken_comp(c.b, index);
}

void do_indent(SubGrid<Color> grid, bool is_corner, bool inverse) {
    assert(grid.width() == k_tile_size && grid.height() == k_tile_size);
    using ModColor = void (*)(Color &);

    std::array<ModColor, 4> mod_colors = {
        [](Color & c) { return darken_color(c, 0); },
        [](Color & c) { return darken_color(c, 1); },
        [](Color & c) { return darken_color(c, 2); },
        [](Color & c) { return darken_color(c, 3); }
    };
    if (inverse) {
        std::swap(mod_colors[0], mod_colors[3]);
        std::swap(mod_colors[1], mod_colors[2]);
    }
    static constexpr const int k_grad = 1;
    auto get_func_index = [](VectorI r, bool is_corner) {

        if (r.x / k_grad == 0 || (is_corner && r.y / k_grad == 0)) {
            return 0;
        } else if (r.x / k_grad == 1 || (is_corner && r.y / k_grad == 1)) {
            return 1;
        } else if (r.x / k_grad == 2 || (is_corner && r.y / k_grad == 2)) {
            return 2;
        } else {
            return 3;
        }
    };
    if (k_grad == 4) {
        assert(get_func_index(VectorI( 2,  4), false) == 0);
        assert(get_func_index(VectorI( 2, 10), false) == 0);
        assert(get_func_index(VectorI( 6,  8), false) == 1);
        assert(get_func_index(VectorI(10,  8), false) == 2);
        assert(get_func_index(VectorI(12,  8), false) == 3);

        assert(get_func_index(VectorI( 2, 13), true ) == 0);
        assert(get_func_index(VectorI( 6,  2), true ) == 0);
        assert(get_func_index(VectorI( 6, 13), true ) == 1);
        assert(get_func_index(VectorI( 8,  6), true ) == 1);
        assert(get_func_index(VectorI(13, 10), true ) == 2);
        assert(get_func_index(VectorI(13, 13), true ) == 3);
    }
    for (VectorI r; r != grid.end_position(); r = grid.next(r)) {
        mod_colors[get_func_index(r, is_corner)](grid(r));
    }
}

void rotate_90(SubGrid<Color> grid) {
    assert(grid.width() == grid.height());
    auto grid_size = grid.width();
    for (int x = 0; x != grid_size / 2    ; ++x) {
    for (int y = x; y != grid_size - x - 1; ++y) {
        auto t = grid(x, y);

        grid(x, y) = grid(y, grid_size - 1 - x);
        grid(y, grid_size - 1 - x) = grid(grid_size - 1 - x, grid_size - 1 - y);
        grid(grid_size - 1 - x, grid_size - 1 - y) = grid(grid_size - 1 - y, x);
        grid(grid_size - 1 - y, x) = t;
    }}
}

void swap_grid_contents(SubGrid<Color> a, SubGrid<Color> b) {
    assert(a.width() == b.width() && a.height() == b.height());
    for (VectorI r; r != a.end_position(); r = b.next(r)) {
        std::swap(a(r), b(r));
    }
}

} // end of <anonymous> namespace
