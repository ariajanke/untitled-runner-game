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

#include "Systems.hpp"
#include "GameDriver.hpp"
#include "GenBuiltinTileSet.hpp"

#include "maps/MapLinks.hpp"
#include "components/Platform.hpp"

#include <tmap/TiledMap.hpp>

#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Sprite.hpp>

#include <common/DrawRectangle.hpp>
#include <common/ParseOptions.hpp>
#include <common/SubGrid.hpp>

#include <iostream>
#include <future>
#include <thread>
#include <tuple>

#include <cassert>

// bouncey items
// falling sliding things
// our hero
// badniks
// gems + hearts
// projectiles
// springs

// to start
// our hero
// springs
// gems

#if 0
struct VectorIHasher {
    std::size_t operator () (const VectorI & r) const {
        return std::size_t(r.x ^ (r.y << 16));
    }
};
#endif

constexpr const char * const k_mark =
  // 0123456789ABCDEF
    "                " // 0
    "                " // 1
    "                " // 2
    "                " // 3
    "                " // 4
    "                " // 5
    "                " // 6
    "                " // 7
    "                " // 8
    "                " // 9
    "                " // A
    "                " // B
    "                " // C
    "                " // D
    "                " // E
    "                ";// F

class Decor : public sf::Drawable {
public:
    virtual std::unique_ptr<Decor> transform() { return nullptr; }
    virtual void setup(std::default_random_engine &) = 0;
    virtual void update(double) = 0;
};

template <typename T>
class FutureDecor final : public Decor {
public:
    static_assert(std::is_base_of_v<Decor, T>, "");

    using TDecorPtr = std::unique_ptr<T>;

    /// @return a promise for another thread to fullfil
    std::promise<TDecorPtr> setup_async() {
        std::promise<TDecorPtr> rv;
        m_future = rv.get_future();
    }

private:

    std::unique_ptr<Decor> transform() override {
        return m_future.valid() ? m_future.get() : nullptr;
    }

    void setup(std::default_random_engine & rng) override {
        m_rng = rng;
        rng.discard(1);
    }

    void update(double) override {}
    void draw(sf::RenderTarget &, sf::RenderStates) const override {}

    std::default_random_engine m_rng;
    std::future<TDecorPtr> m_future;
};

class Gen {
public:
    static constexpr const int k_fps = 6;
    void setup();
    void update(double);
    void render_to(sf::RenderTarget &) const;
private:
    sf::Texture m_output;
    std::vector<sf::Texture> m_textures;
    std::size_t m_index = 0;
    double m_total_et = 0.;
};

sf::Vector2f compute_view_for_window
    (int win_width, int win_height, int display_width, int display_height)
{
    sf::Vector2f view_size;
    if (display_width / win_width >= display_height / win_height) {
        int scale = display_height / win_height;
        int rem = display_height % win_height;
        view_size.y = (float(rem) / float(scale)) + float(win_height);
        view_size.x = (float(display_width) / float(display_height))*view_size.y;
    } else {
        int scale = display_width / win_width;
        int rem = display_width % win_width;
        view_size.x = (float(rem) / float(scale)) + float(win_width);
        view_size.y = (float(display_height) / float(display_width))*view_size.x;
    }
    if (!(view_size.x >= float(win_width) && view_size.y >= float(win_height))) {
        return compute_view_for_window(win_width, win_height, display_width, display_height);
    }
    assert(view_size.x >= float(win_width) && view_size.y >= float(win_height));
    return view_size;
}

void load_test_map(StartupOptions &, char ** beg, char ** end);
void save_builtin_tileset(StartupOptions &, char ** beg, char ** end);

int main(int argc, char ** argv) {
    std::cout << "Component table size " << Entity::k_component_table_size
              << " bytes.\nNumber of inlined components "
              << Entity::k_number_of_components_inlined << "." << std::endl;

    InterpolativePosition::run_tests();
    {
    DefineInPlaceVector<int, 5> ipv; //std::vector<int, DefineInPlace<4>::Allocator<int>> ipv;
    reserve_in_place(ipv);
    std::cout << (sizeof(ipv) / sizeof(void *)) << ":" << (sizeof(ipv) % sizeof(void *)) << std::endl;
    ipv.push_back(1);
    }
    MapLinks::run_tests();
    std::cout << &k_gravity << std::endl;

    StartupOptions opts = parse_options<StartupOptions>(argc, argv, {
        { "test-map"            , 'm', load_test_map        },
        { "save-builtin-tileset", 's', save_builtin_tileset }
    });

    {
    PlayerControl pc;
    pc.direction = PlayerControl::k_left_only;
    press_right(pc);
    assert(pc.direction == PlayerControl::k_right_last);
    release_right(pc);
    assert(pc.direction == PlayerControl::k_left_only);

    pc.direction = PlayerControl::k_left_only;
    press_right(pc);
    release_left(pc);
    assert(pc.direction == PlayerControl::k_right_only);
    }

    sf::RenderWindow win;
    GameDriver gdriver;
    Gen gen;

    EnvironmentCollisionSystem::run_tests();

    win.create(sf::VideoMode(k_view_width*3, k_view_height*3), "Bouncy Bouncy UwU");
    win.setKeyRepeatEnabled(false);
    win.setFramerateLimit(60);
    gdriver.setup(opts, win.getView());
    gen.setup();

    bool frame_advance_enabled = false;
    bool do_this_frame         = true ;
    sf::Clock clock;
    while (win.isOpen()) {
        sf::Event event;
        while (win.pollEvent(event)) {
            switch (event.type) {
            case sf::Event::Closed:
                win.close();
                break;
            case sf::Event::KeyReleased:
                switch (event.key.code) {
                case sf::Keyboard::Escape:
                    win.close();
                    break;
                case sf::Keyboard::Q:
                    frame_advance_enabled = !frame_advance_enabled;
                    do_this_frame = true;
                    break;
                case sf::Keyboard::E:
                    do_this_frame = true;
                    break;
                default: break;
                }
                break;
            case sf::Event::Resized: {
                sf::View v = win.getView();
                v.setSize(compute_view_for_window(k_view_width, k_view_height, int(win.getSize().x), int(win.getSize().y)));
                win.setView(v);
                }
                break;
            default: break;
            }
            gdriver.process_event(event);
        }
        win.clear(sf::Color(100, 100, 255));
        if (do_this_frame) {
            gdriver.update(double(clock.restart().asSeconds()));// 1. / 60.);
        }
        if (do_this_frame && frame_advance_enabled) {
            do_this_frame = false;
        }


        {
        const auto old_view = win.getView();
        auto new_view = old_view;
        new_view.setCenter(sf::Vector2f(gdriver.camera_position()));
        win.setView(new_view);
        gdriver.render_to(win);
#       if 1
        gen.render_to(win);
#       endif
#       if 0
        {
        auto pl = gdriver.get_player();
        if (pl.has<LineMap::ConstGridRangeType>()) {
            auto range = pl.component<LineMap::ConstGridRangeType>();
            DrawRectangle drect;
            auto beg_pos = range.begin_position();
            auto sz_v    = range.last_position() - beg_pos + VectorI(1, 1);
            drect.set_position(beg_pos.x*16.f, beg_pos.y*16.f);
            drect.set_size    (sz_v.x   *16.f, sz_v.y   *16.f);
            win.draw(drect);
        }
        }
#       endif

        auto hud_view = old_view;
        hud_view.setCenter(hud_view.getSize().x / 2.f, hud_view.getSize().y / 2.f);
        win.setView(hud_view);
        gdriver.render_hud_to(win);
        }
        gen.update(1./60.);
        win.display();
    }
    return 0;
}

// ----------------------------------------------------------------------------

void load_test_map(StartupOptions & opts, char ** beg, char ** end) {
    if (end - beg < 1) {
        throw std::runtime_error("test-map requires at least one argument");
    }
    opts.test_map = std::string(*beg);
}

void save_builtin_tileset(StartupOptions &, char ** beg, char ** end) {
    if (beg == end) {
        throw std::runtime_error("must specify filename to save to");
    }
    to_image(generate_atlas()).saveToFile(*beg);
}

template <typename T, typename Urng>
T choose_random(std::initializer_list<T> list, Urng & rng) {
    return *(list.begin() + std::uniform_int_distribution<int>(0, list.size() - 1)(rng));
}

sf::Color darken(sf::Color c, uint8_t u) {
    for (auto * cptr : { &c.r, &c.g, &c.b }) {
        *cptr = (*cptr < u) ? 0 : *cptr - u;
    }
    return c;
}

Grid<sf::Color> gen_cloud
    (int w, int h, double density, int min_r, std::default_random_engine & rng)
{
    using IntDistri = std::uniform_int_distribution<int>;
    assert(min_r < h / 2);
    assert(w > h);
    const int cloud_count = int(double(w*h)*density);
    Grid<sf::Color> rv;
    rv.set_size(w, h, sf::Color(0, 0, 0, 0));
    for (int i = 0; i != cloud_count; ++i) {
        int radius = IntDistri(min_r, h / 2)(rng);
        VectorI center(IntDistri(radius, w - 1 - radius)(rng), h - radius);
        for (VectorI r; r != rv.end_position(); r = rv.next(r)) {
            auto diff = center - r;
            if ((diff.x*diff.x + diff.y*diff.y) > radius*radius) continue;
            rv(r) = sf::Color::White;
        }
    }
    return rv;
}

void flip_horizontally(SubGrid<sf::Color> grid) {
    for (int y = 0; y != grid.height()    ; ++y) {
    for (int x = 0; x != grid.width () / 2; ++x) {
        std::swap(grid(x, y), grid(grid.width() - x - 1, y));
    }}
}

void flip_vertically(SubGrid<sf::Color> grid) {
    for (int y = 0; y != grid.height() / 2; ++y) {
    for (int x = 0; x != grid.width ()    ; ++x) {
        std::swap(grid(x, y), grid(x, grid.height() - y - 1));
    }}
}

template <typename T>
bool is_inside_triangle
    (const sf::Vector2<T> & a, const sf::Vector2<T> & b,
     const sf::Vector2<T> & c, const sf::Vector2<T> & test_point)
{
    // derived from mathematics presented here:
    // https://blackpawn.com/texts/pointinpoly/default.html
    const auto & p = test_point;
    // convert to Barycentric cordinates
    auto ca = c - a;
    auto ba = b - a;
    auto pa = p - a;

    auto dot_caca = dot(ca, ca);
    auto dot_caba = dot(ca, ba);
    auto dot_capa = dot(ca, pa);
    auto dot_baba = dot(ba, ba);
    auto dot_bapa = dot(ba, pa);

    auto denom = dot_caca*dot_baba - dot_caba*dot_caba;
    auto u = dot_baba*dot_capa - dot_caba*dot_bapa;
    auto v = dot_caca*dot_bapa - dot_caba*dot_capa;

    return u >= 0 && v >= 0 && (u + v < denom);
}

template <typename T>
sf::Rect<T> find_rectangle_bounds
    (const sf::Vector2<T> & a, const sf::Vector2<T> & b, const sf::Vector2<T> & c)
{
    static constexpr const auto k_low  = std::numeric_limits<T>::min();
    static constexpr const auto k_high = std::numeric_limits<T>::max();

    auto max_x = k_low;
    auto min_x = k_high;
    for (auto x : { a.x, b.x, c.x }) {
        max_x = std::max(max_x, x);
        min_x = std::min(min_x, x);
    }
    auto max_y = k_low;
    auto min_y = k_high;
    for (auto y : { a.y, b.y, c.y }) {
        max_y = std::max(max_y, y);
        min_y = std::min(min_y, y);
    }
    return sf::Rect<T>(min_x, min_y, max_x - min_x, max_y - min_y);
}

void extend_int_triangle(VectorI & a, VectorI & b, VectorI & c) {
    static auto norm_int_vec = [](VectorI r) {
        return VectorI(r.x == 0 ? 0 : normalize(r.x),
                       r.y == 0 ? 0 : normalize(r.y));
    };
    VectorI a_ex = a - ((b + c) / 2);
    VectorI b_ex = b - ((a + c) / 2);
    VectorI c_ex = c - ((b + a) / 2);
    a += norm_int_vec(a_ex);
    b += norm_int_vec(b_ex);
    c += norm_int_vec(c_ex);
}

Grid<sf::Color> add_bottom_ridges(const Grid<sf::Color> & grid) {
    Grid<sf::Color> rv;
    rv.set_size(grid.width(), grid.height());
    for (VectorI r; r != grid.end_position(); r = grid.next(r)) {
        rv(r) = grid(r);
    }
#   if 0

    for (int y = rv.height() - 16; y != rv.height(); ++y) {
    for (int x = 0; x != rv.width (); ++x) {
        rv(x, y) = sf::Color(0, 0, 0, 0);
    }}
#   endif
    static constexpr const int k_ridge_size = 4;
    static const VectorI k_uninit(-1, -1);
    VectorI last = k_uninit;
    for (VectorI r; r != rv.end_position(); r = rv.next(r)) {
        VectorI lower = r + VectorI(0, 1);
        if (!rv.has_position(lower)) continue;
        if (rv(lower) != sf::Color(0, 0, 0, 0) ||
            rv(lower) == rv(r))
        { continue; }
        if (last == k_uninit) {
            last = r;
            continue;
        }
        if (r.x != last.x + k_ridge_size)
            { continue; }

        VectorI other   = ((r + last) / 2) + VectorI(0, 6);
        VectorI ex_r    = r;
        VectorI ex_last = last;
        extend_int_triangle(other, ex_r, ex_last);
        auto bounds = find_rectangle_bounds(other, last, r);
        for (int y = bounds.top ; y != bottom_of(bounds); ++y) {
        for (int x = bounds.left; x != right_of (bounds); ++x) {
            if (is_inside_triangle(other, ex_last, ex_r, VectorI(x, y))) {
                rv(x, y) = sf::Color::Red;
            }
        }}
        last = r;
    }
    return rv;
}

void half_rotate_horizontally(SubGrid<sf::Color> grid) {
    for (int y = 0; y != grid.height()    ; ++y) {
    for (int x = 0; x != grid.width () / 2; ++x) {
        std::swap(grid(x, y), grid(x + grid.width() / 2, y));
    }}
}

// two sets of shades of color
// need grass, waterfalls
// indents
// I want a loop, 1x1, 2x1, 3x1, 4x1, 5x1 slopes
// flat surfaces, ceilings, floors, walls
// deco tiles indents and their corners and edges
// highlight edges in some fashion
// other decor vines, flowers, trees
// background sky, clouds, sun(?)
// water, waterfalls

template <typename T>
T magnitude(const sf::Vector3<T> & r) {
    return T(std::sqrt(r.x*r.x + r.y*r.y + r.z*r.z));
}

Grid<sf::Color> gen_shaded_circle(int radius, sf::Vector3<int> light_pos) {
    using Vec3 = sf::Vector3<int>;
    Grid<sf::Color> rv;
    rv.set_size(radius*2, radius*2);
    for (VectorI r; r != rv.end_position(); r = rv.next(r)) {
        if (magnitude(VectorD(VectorI(radius, radius) - r)) < radius) {
            auto dist = magnitude(light_pos - Vec3(r.x, r.y, 0));
            //constexpr const auto k_min_dist ;
            auto min_color = sf::Color(25, 25, 25);
            auto max_color = sf::Color::White;
            auto min_dist  = 180;
            auto max_dist  = 3000;
            const auto min_dist_val = 1. / (min_dist*min_dist);
            const auto max_dist_val = 1. / (max_dist*max_dist);
            auto dist_val = 1. / double(dist*dist);
            dist_val = std::min(std::max(dist_val, max_dist_val), min_dist_val);
            auto porpro = (dist_val - max_dist_val) / (min_dist_val - max_dist_val);
            auto color_val = 80. + porpro*(255. - 80.);
            //color_val = uint8_t( std::max(std::min(color_val, 1.), 0.));
            rv(r) = sf::Color(color_val, color_val, color_val);
        } else {
            rv(r) = sf::Color(0, 0, 0, 0);
        }
    }
    return rv;
}

template <typename T>
struct VecHash {
    std::size_t operator () (const sf::Vector2<T> & r) const {
        std::hash<T> hashf;
        return hashf(r.x) ^ hashf(r.y + 12467);
    }
};

void do_line_circle_segments(ConstSubGrid<sf::Color>, const std::vector<LineSegment> & segments) {
    if (segments.empty()) return;
    std::unordered_map<VectorI, std::vector<LineSegment>, VecHash<int>> seg_tile_map;
    static auto vec_tile_loc = [](VectorD r) { return VectorI(int(r.x) / 16, int(r.y) / 16); };
    static auto add_seg = [](decltype (seg_tile_map) & map, LineSegment seg) {
        assert(vec_tile_loc(seg.a) == vec_tile_loc(seg.b));
        auto tile_loc = vec_tile_loc(seg.a);
        VectorD offset = VectorD(tile_loc)*16.;
        seg.a -= offset;
        seg.b -= offset;
        // now normalize
        seg.a /= 16.;
        seg.b /= 16.;
        auto itr = map.find(tile_loc);
        std::vector<LineSegment> * vec = nullptr;
        if (itr == map.end()) {
            vec = &map[tile_loc];
            vec->reserve(16);
        } else {
            vec = &itr->second;
        }
        vec->push_back(seg);
    };
    for (const auto & seg : segments) {
        if (vec_tile_loc(seg.a) == vec_tile_loc(seg.b)) {
            add_seg(seg_tile_map, seg);
        } else {
            VectorD a_end;
        }
    }

    VectorI max_pos(-1, -1);
    for (auto & pair : seg_tile_map) {
        max_pos.x = std::max(max_pos.x, pair.first.x);
        max_pos.y = std::max(max_pos.y, pair.first.y);
    }

    auto to_id = [max_pos](VectorI r) { return r.x + r.y*(max_pos.x + 1); };
    std::map<int, std::vector<LineSegment>> seg_map;
    for (auto & pair : seg_tile_map) {
        seg_map[to_id(pair.first)].swap(pair.second);
    }
    // add extreme points
    auto add_extreme = [&seg_map, &to_id](VectorI tile, VectorD corner) {
        auto & vec = seg_map[to_id(tile)];
        static const VectorD k_uninit(10000, 10000);
        VectorD closest = k_uninit;
        for (const auto & seg : vec) {
            for (auto pt : { seg.a, seg.b }) {
                if (magnitude(pt - corner) < magnitude(pt - closest)) {
                    closest = pt;
                }
            }
        }
        assert(closest != k_uninit);
        vec.push_back(LineSegment(corner, closest));
    };
    add_extreme(VectorI(max_pos.x / 2    , max_pos.y), VectorD(1, 1));
    add_extreme(VectorI(max_pos.x / 2 + 1, max_pos.y), VectorD(0, 1));

    add_extreme(VectorI(max_pos.x / 2    , 0), VectorD(1, 0));
    add_extreme(VectorI(max_pos.x / 2 + 1, 0), VectorD(0, 0));

    add_extreme(VectorI(0, max_pos.y / 2    ), VectorD(0, 1));
    add_extreme(VectorI(0, max_pos.y / 2 + 1), VectorD(0, 0));

    add_extreme(VectorI(max_pos.x, max_pos.y / 2    ), VectorD(1, 1));
    add_extreme(VectorI(max_pos.x, max_pos.y / 2 + 1), VectorD(1, 0));
    for (auto & pair : seg_tile_map) {
        // sort all the vectors
        auto & vec = pair.second;
        std::sort(vec.begin(), vec.end(), []
            (const LineSegment & lhs, const LineSegment & rhs)
        { return lhs.a.x < rhs.a.x; });
    }
#   if 0
    if (seg_map.empty()) return;
    static constexpr const char * k_head =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<tileset name=\"5x5loop\" tilewidth=\"16\" tileheight=\"16\" tilecount=\"256\" columns=\"16\">"
             "<image source=\"5x5loop.png\" width=\"160\" height=\"176\"/>";
    static constexpr const char * k_foot ="</tileset>";
    std::cout << k_head << std::endl;
    for (const auto & pair : seg_map) {
        std::cout << "<tile id=\"" << pair.first
                  <<"\"><properties><property name=\"lines\" value=\"";

        assert(!pair.second.empty());
        const auto * last = &*(pair.second.end() - 1);
        for (const auto & seg : pair.second) {
            std::cout << seg.a.x << ", " << seg.a.y << " : " << seg.b.x << ", " << seg.b.y;
            if (&seg != last) {
                std::cout << ";";
            }
        }
        std::cout << "\"/></properties></tile>" << std::endl;
    }
    std::cout << k_foot << std::endl;
#   endif
}

void Gen::setup() {
}

void Gen::update(double et) {
    if (m_textures.empty()) return;
    static const constexpr auto k_spf = 1. / double(k_fps*2.);
    m_total_et += et;
    if (m_total_et > k_spf) {
        m_total_et -= k_spf;
        m_index = (m_index + 1) % m_textures.size();
    }
}

void Gen::render_to(sf::RenderTarget & target) const {
//    return;
    if (m_textures.empty()) return;
    sf::Sprite spt;
    spt.setPosition(22.f*16.f, 19.f*16.f);
    spt.setTexture(m_textures[m_index]);
    target.draw(spt);
}
