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
#include "SubGrid.hpp"

#include "maps/MapLinks.hpp"

#include <tmap/TiledMap.hpp>

#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Sprite.hpp>

#include <common/DrawRectangle.hpp>
#include <common/ParseOptions.hpp>

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

class DisplacementIterator {
public:
    DisplacementIterator() {}
    DisplacementIterator(VectorD total, double max):
        m_total(total), m_max(max) {}

    bool operator == (const DisplacementIterator & rhs) const {
        return m_total == rhs.m_total && m_max == rhs.m_max;
    }

    void operator ++ () {
        if (magnitude(m_total) > m_max) {
            m_total -= step();
        } else {
            m_total = VectorD();
            m_max   = 0.;
        }
    }

    VectorD operator * () const { return step(); }

private:
    VectorD step () const
        { return normalize(m_total)*m_max; }
    VectorD m_total;
    double m_max = 0.;
};

class DisplacementRange {
public:
    DisplacementRange(VectorD total, double max);

    DisplacementIterator begin() const
        { return DisplacementIterator(m_total, m_max); }

    DisplacementIterator end() const
        { return DisplacementIterator(); }
private:
    VectorD m_total;
    double m_max = 0.;
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

int main(int argc, char ** argv) {

    {
    DefineInPlaceVector<int, 5> ipv; //std::vector<int, DefineInPlace<4>::Allocator<int>> ipv;
    reserve_in_place(ipv);
    std::cout << (sizeof(ipv) / sizeof(void *)) << ":" << (sizeof(ipv) % sizeof(void *)) << std::endl;
    ipv.push_back(1);
    }
    MapLinks::run_tests();
    std::cout << &k_gravity << std::endl;

    StartupOptions opts = parse_options<StartupOptions>(argc, argv, {
        { "test-map", 'm', load_test_map }
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
            gdriver.update(1. / 60.);
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

        win.setView(old_view);
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

inline sf::Color mk_color(int hex) {
    return sf::Color(hex >> 16, (hex >> 8) & 0xFF, hex & 0xFF);
}

template <typename T>
T get(std::initializer_list<T> list, int x) {
    return *(list.begin() + x);
}

template <typename T, typename Urng>
T choose_random(std::initializer_list<T> list, Urng & rng) {
    return *(list.begin() + std::uniform_int_distribution<int>(0, list.size() - 1)(rng));
}

sf::Image to_image(const Grid<sf::Color> & grid) {
    sf::Image img;
    img.create(unsigned(grid.width()), unsigned(grid.height()));
    for (VectorI r; r != grid.end_position(); r = grid.next(r)) {
        img.setPixel(unsigned(r.x), unsigned(r.y), grid(r));
    }
    return img;
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

void rotate_90(SubGrid<sf::Color> grid) {
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

void darken_color(sf::Color & c, int index) {
    static auto darken_comp = [](uint8_t & c, int mul) {
        if (mul == 0) return;
        int res = c - 30 - (mul - 1)*20;
        c = uint8_t(std::max(0, res));
    };
    darken_comp(c.r, index);
    darken_comp(c.g, index);
    darken_comp(c.b, index);
}

void do_indent(SubGrid<sf::Color> grid, bool is_corner, bool inverse) {
    assert(grid.width() == 16 && grid.height() == 16);
    using ModColor = void (*)(sf::Color &);

    std::array<ModColor, 4> mod_colors = {
        [](sf::Color & c) { return darken_color(c, 0); },
        [](sf::Color & c) { return darken_color(c, 1); },
        [](sf::Color & c) { return darken_color(c, 2); },
        [](sf::Color & c) { return darken_color(c, 3); }
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

Grid<sf::Color> gen_waterfall
    (int w, int h, int pal_rotation_idx, int x_)
{
    //x_ = (x_ % 2) + 1;
    Grid<int> preimage;
    preimage.set_size(w, h);
    static constexpr const int k_max_colors = 4;
    static constexpr const int k_max_height = 16;

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
    Grid<sf::Color> image;
    image.set_size(w, h);
    for (VectorI r; r != image.end_position(); r = image.next(r)) {
        image(r) = get(k_palette, (preimage(r) + pal_rotation_idx) % k_max_colors);
        //image(r).a = 100;
    }
    return image;
}

void add_grass(SubGrid<sf::Color> grid, std::default_random_engine & rng) {
    using IntDistri = std::uniform_int_distribution<int>;
    static const auto k_greens = {
        mk_color(0x008000),
        mk_color(0x00A000),
        mk_color(0x00C000),
        mk_color(0x20D820),
    };
    for (VectorI r; r != grid.end_position(); r = grid.next(r)) {
        VectorI upper = r + VectorI(0, -1);
        if (!grid.has_position(upper)) continue;
        if (grid(upper) == sf::Color(0, 0, 0, 0) &&
            grid(upper) != grid(r))
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
                if (!grid.has_position(r - VectorI(0, yd))) continue;
                auto & c = grid(r - VectorI(0, yd));
                if (yd < grass_start) {
                    darken_color(c, 2);
                } else if (yd <= 0) {
                    c = get(k_greens, 0 + offset);
                } else if (deep_over > 0) {
                    --deep_over;
                    c = get(k_greens, 0 + offset);
                } else if(mid_over > 0) {
                    --mid_over;
                    c = get(k_greens, 1 + offset);
                } else {
                    c = get(k_greens, 2 + offset);
                }
            }
        }
    }
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

std::array<VectorI, 4> get_mirror_points(ConstSubGrid<sf::Color> grid, VectorI r) {
    return {
        r,
        VectorI(grid.width() - r.x - 1,                 r.y    ),
        VectorI(               r.x    , grid.height() - r.y - 1),
        VectorI(grid.width() - r.x - 1, grid.height() - r.y - 1)
    };
}

std::array<VectorI, 4> get_mirror_points(ConstSubGrid<sf::Color> grid, int x, int y) {
    return get_mirror_points(grid, VectorI(x, y));
}

// assumes four way symetric
void add_border(SubGrid<sf::Color> grid) {
    using std::make_pair;
    static const sf::Color k_light = mk_color(0x906000);
    static const sf::Color k_dark  = mk_color(0x603000);
    static const sf::Color k_trnsp(0, 0, 0, 0);

    auto safe_write = [&grid](VectorI r) -> sf::Color & {
        static sf::Color badref;
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
            if (grid(r) == k_trnsp && grid(v) != k_trnsp) {
                x_ways = -1;
            }
        }
        if (grid.has_position(l)) {
            if (grid(l) == k_trnsp && grid(v) != k_trnsp) {
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
            if (grid(t) == k_trnsp && grid(v) != k_trnsp) {
                y_ways = 1;
            }
        }
        if (grid.has_position(b)) {
            if (grid(b) == k_trnsp && grid(v) != k_trnsp) {
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

void half_rotate_horizontally(SubGrid<sf::Color> grid) {
    for (int y = 0; y != grid.height()    ; ++y) {
    for (int x = 0; x != grid.width () / 2; ++x) {
        std::swap(grid(x, y), grid(x + grid.width() / 2, y));
    }}
}

void cut_circle(SubGrid<sf::Color> grid, bool as_hole) {

    auto radius = std::min(grid.width(), grid.height()) / 2;
    VectorI center(grid.width() / 2, grid.height() / 2);
    for (int y = 0; y != grid.height() / 2; ++y) {
    for (int x = 0; x != grid.width () / 2; ++x) {
        auto diff = center - VectorI(x, y);
        bool is_inside = (diff.x*diff.x + diff.y*diff.y) < radius*radius;
        if ((is_inside && as_hole) || (!is_inside && !as_hole)) {
            for (const auto & pt : get_mirror_points(grid, x, y))
                { grid(pt) = sf::Color(0, 0, 0, 0); }
        }
    }}
#   if 0
    auto bump_color = as_hole ? sf::Color(0, 0, 0, 0) : sf::Color::White;

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
    auto one_neighbor_is = [grid](VectorI r, bool (*f)(sf::Color)) {
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
        if (!(x % 4 == 0 || y % 16 == 0 || y % 16 == 15)) continue;
        static const sf::Color k_t(0, 0, 0, 0);
        VectorI r(x, y);
        auto transparent = [](sf::Color c){ return c == k_t; };
        auto opaque      = [](sf::Color c){ return c != k_t; };
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

void cut_diamond(SubGrid<sf::Color> grid, bool as_hole) {
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
                { grid(pt) = sf::Color(0, 0, 0, 0); }
        }
    }}
}

const Grid<sf::Color> & get_base_checkerboard() {
    static Grid<sf::Color> rv;
    if (!rv.is_empty()) return rv;

    constexpr const int k_width  = 16;
    constexpr const int k_height = 16;
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

void swap_grid_contents(SubGrid<sf::Color> a, SubGrid<sf::Color> b) {
    assert(a.width() == b.width() && a.height() == b.height());
    for (VectorI r; r != a.end_position(); r = b.next(r)) {
        std::swap(a(r), b(r));
    }
}

Grid<sf::Color> gen_all_indents() {
    auto grid = get_base_checkerboard();

    auto tile_width = grid.width();
    auto tile_height = grid.height();
#   if 0
    if (false){
    ConstSubGrid<sf::Color> base(grid, 8, 8);
    SubGrid<sf::Color> a(grid, VectorI(0, 8), 8, 8);
    SubGrid<sf::Color> b(grid, VectorI(8, 0), 8, 8);
    for (auto * g : { &a, &b }) {
        for (VectorI r; r != base.end_position(); r = base.next(r)) {
            (*g)(r) = base(r);
        }
    }
    }
#   endif

    auto get_hexdecant = [&grid, tile_width, tile_height](int x, int y) {
        return SubGrid<sf::Color>(grid, VectorI(x*tile_width, y*tile_height), tile_width, tile_height);
    };
    auto gen_set = [tile_width, tile_height](SubGrid<sf::Color> grid) {
        auto ff = grid.make_sub_grid(tile_width, tile_height); //SubGrid<sf::Color> ff(grid, tile_width, tile_height);
        do_indent(ff, false, false);

        auto ft = grid.make_sub_grid(VectorI(0, tile_height), tile_width, tile_height);// SubGrid<sf::Color> ft(grid, VectorI(0, tile_height), tile_width, tile_height);
        do_indent(ft, false, true);

        auto tf = grid.make_sub_grid(VectorI(tile_width, 0), tile_width, tile_height);// SubGrid<sf::Color> tf(grid, VectorI(tile_width, 0), tile_width, tile_height);
        do_indent(tf, true, false);

        auto tt = grid.make_sub_grid(VectorI(tile_width, tile_height), tile_width, tile_height);// SubGrid<sf::Color> tt(grid, VectorI(tile_width, tile_height), tile_width, tile_height);
        do_indent(tt, true, true);
    };

    grid = extend(grid, 4, 4);
    auto quad_width = tile_width*2;
    auto quad_height = tile_height*2;
    {
    SubGrid<sf::Color> g(grid, VectorI(), quad_width, quad_height);

    gen_set(g);

    }
    {
    SubGrid<sf::Color> g(grid, VectorI(quad_width, 0), quad_width, quad_height);
    rotate_90(g);
    gen_set(g);
    rotate_90(g);
    rotate_90(g);
    rotate_90(g);
    }

    {
    SubGrid<sf::Color> g(grid, VectorI(0, quad_height), quad_width, quad_height);
    rotate_90(g);
    rotate_90(g);
    gen_set(g);
    rotate_90(g);
    rotate_90(g);

    }
    {
    SubGrid<sf::Color> g(grid, VectorI(quad_width, quad_height), quad_width, quad_height);
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

Grid<sf::Color> push_down(ConstSubGrid<sf::Color> grid, int x_line, int y_amount) {
    static constexpr const int k_rest = SubGrid<sf::Color>::k_rest_of_grid;
    Grid<sf::Color> rv;
    rv.set_size(grid.width(), grid.height());
    SubGrid<sf::Color> head(rv                               , k_rest, x_line  );
    SubGrid<sf::Color> body(rv, VectorI(0, x_line           ), k_rest, y_amount);
    SubGrid<sf::Color> foot(rv, VectorI(0, x_line + y_amount), k_rest, k_rest  );
    for (VectorI r; r != head.end_position(); r = head.next(r)) {
        head(r) = grid(r);
    }
    for (VectorI r; r != body.end_position(); r = body.next(r)) {
        body(r) = sf::Color(0, 0, 0, 0);
    }
    auto footw = foot.width();
    auto footh = foot.height();
    for (VectorI r; r != foot.end_position(); r = foot.next(r)) {
        foot(r) = grid(r + VectorI(0, x_line));
    }
    return rv;
}

Grid<sf::Color> gen_flats(std::default_random_engine & rng) {
    static constexpr const int k_size = 16;
    const auto & base = get_base_checkerboard();
    Grid<sf::Color> preimage;
    preimage.set_size(5*k_size, 5*k_size, sf::Color(0, 0, 0, 0));
    for (int y = k_size; y != 4*k_size; ++y) {
    for (int x = k_size; x != 4*k_size; ++x) {
        preimage(x, y) = base(x % k_size, y % k_size);
    }}
    add_border(preimage);
    add_grass(preimage, rng);

    SubGrid<sf::Color> dark_tile(preimage, VectorI(2*k_size, 2*k_size), k_size, k_size);
    for (VectorI r; r != dark_tile.end_position(); r = dark_tile.next(r)) {
        darken_color(dark_tile(r), 3);
    }

    Grid<sf::Color> rv;
    rv.set_size(3*k_size, 4*k_size);
    ConstSubGrid<sf::Color> imgin(preimage, VectorI(k_size, 0), k_size*3, k_size*4);
    for (VectorI r; r != rv.end_position(); r = rv.next(r)) {
        rv(r) = imgin(r);
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

#   if 1
    const auto seed = std::random_device()();
    {
    using std::make_pair;
    get_base_checkerboard();
#   if 0
    std::default_random_engine rng { seed };
    to_image(gen_all_indents()).saveToFile("/media/ramdisk/indents.png");
#   endif
    using ProdFunc = void (*)(SubGrid<sf::Color>, bool);
    auto gen_slopes_ = [&seed](int width, int height, ProdFunc pf, bool as_hole) {
        std::default_random_engine rng { seed };
        static constexpr const int k_rest_of = SubGrid<sf::Color>::k_rest_of_grid;

        auto g = get_base_checkerboard();
        g = extend(g, width*2,
                   height*2 + (as_hole && pf == cut_circle ? 0 : 1) + (as_hole ? 1 : 0));
        pf        (SubGrid<sf::Color>(g, k_rest_of, height*2*16), as_hole);

        add_border(SubGrid<sf::Color>(g, k_rest_of, height*2*16));

        if (pf == cut_diamond || (!as_hole && pf == cut_circle))
            g = push_down(g, as_hole ? height*16 : 0, 16);

        SubGrid<sf::Color> grass_seg;
        if (as_hole) {
            grass_seg = SubGrid<sf::Color>(g, VectorI(0, height*16), k_rest_of, k_rest_of);
        } else {
            grass_seg = SubGrid<sf::Color>(g, k_rest_of, (height + 1)*16);
        }

        add_grass               (grass_seg, rng);
        return g;
    };

    auto bottom_half = [&gen_slopes_](int width, int height, ProdFunc pf, bool as_hole) {
        // div 16, round down
        auto g = gen_slopes_(width, height, pf, as_hole);
        Grid<sf::Color> dest;
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
#   if 0
    auto gen_slopes = [&gen_slopes_](int width, int height, ProdFunc pf, bool as_hole, const char * filename) {
        to_image(gen_slopes_(width, height, pf, as_hole)).saveToFile(filename);
    };
    gen_slopes(1, 1, cut_diamond, true, "/media/ramdisk/1x1diamond.png"); //
    gen_slopes(1, 2, cut_diamond, true, "/media/ramdisk/1x2diamond.png"); //
    gen_slopes(1, 3, cut_diamond, true, "/media/ramdisk/1x3diamond.png"); //
    gen_slopes(1, 4, cut_diamond, true, "/media/ramdisk/1x4diamond.png"); //

    gen_slopes(2, 1, cut_diamond, true, "/media/ramdisk/2x1diamond.png"); //
    gen_slopes(3, 1, cut_diamond, true, "/media/ramdisk/3x1diamond.png"); //
    gen_slopes(4, 1, cut_diamond, true, "/media/ramdisk/4x1diamond.png"); //

    gen_slopes(2, 3, cut_diamond, true, "/media/ramdisk/2x3diamond.png"); //
    gen_slopes(3, 2, cut_diamond, true, "/media/ramdisk/3x2diamond.png");

    gen_slopes(5, 5, cut_circle, true, "/media/ramdisk/5x5loop.png");
    gen_slopes(5, 5, cut_circle, false, "/media/ramdisk/5x5circle.png");

    gen_slopes(7, 7, cut_circle, true , "/media/ramdisk/7x7loop.png");
    gen_slopes(7, 7, cut_circle, false, "/media/ramdisk/7x7circle.png");

    gen_slopes(10, 10, cut_circle, true , "/media/ramdisk/10x10loop.png");
    gen_slopes(10, 10, cut_circle, false, "/media/ramdisk/10x10circle.png");

    to_image(gen_flats(rng)).saveToFile("/media/ramdisk/flats.png");

#endif

    using std::bind;
    using std::move;
    using Rng = std::default_random_engine;

    std::vector<std::pair<std::function<Grid<sf::Color>()>, VectorI>>
        task_list
    {
        make_pair(bind(gen_slopes_, 10, 10, cut_circle, true ), VectorI(  0,   0)),
        make_pair(bind(gen_slopes_,  7,  7, cut_circle, false), VectorI( 48,  32)),
        make_pair(bind(gen_slopes_,  7,  7, cut_circle, true ), VectorI(320,   0)),
        make_pair(bind(gen_slopes_,  5,  5, cut_circle, false), VectorI(352,  16)),
        make_pair(bind(gen_slopes_,  5,  5, cut_circle, true ), VectorI(320, 240)),
        make_pair(bind(top_half   , 10, 10, cut_circle, false), VectorI(  0, 336)),
        make_pair(bind(bottom_half, 10, 10, cut_circle, false), VectorI(320, 416)),

        make_pair(bind(gen_flats      ,Rng { seed }), VectorI(544,  64)),
        make_pair(     gen_all_indents              , VectorI(592,  64)),

        make_pair(bind(gen_slopes_, 1, 1, cut_diamond, true), VectorI(576, 288)),
        make_pair(bind(gen_slopes_, 1, 2, cut_diamond, true), VectorI(608, 224)),
        make_pair(bind(gen_slopes_, 1, 3, cut_diamond, true), VectorI(544, 288)),
        make_pair(bind(gen_slopes_, 1, 4, cut_diamond, true), VectorI(640, 128)),
        make_pair(bind(gen_slopes_, 2, 1, cut_diamond, true), VectorI(544, 224)),
        make_pair(bind(gen_slopes_, 3, 1, cut_diamond, true), VectorI(576, 352)),
        make_pair(bind(gen_slopes_, 4, 1, cut_diamond, true), VectorI(544,   0)),
        make_pair(bind(gen_slopes_, 3, 2, cut_diamond, true), VectorI(544, 128)),
        make_pair(bind(gen_slopes_, 2, 3, cut_diamond, true), VectorI(480, 240)),
    };
    using FutureGrid = std::future<Grid<sf::Color>>;
    std::vector<std::pair<FutureGrid, VectorI>> futures;
    futures.reserve(task_list.size());

    std::vector<std::thread> threads;
    threads.reserve(task_list.size());
    for (auto & [task, loc] : task_list) {
        std::packaged_task<Grid<sf::Color>()> pkgtask { std::move(task) };
        futures.emplace_back(make_pair(FutureGrid { pkgtask.get_future() }, loc));
        threads.emplace_back(std::move(pkgtask));
        threads.back().detach();
    }

    for (auto & [future, _] : futures) {
        (void)_;
        future.wait();
    }

    std::vector<std::pair<Grid<sf::Color>, VectorI>> products;
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
    Grid<sf::Color> composite;
    composite.set_size(w, h, sf::Color(0, 0, 0, 0));
    for (const auto & [grid, r] : products) {
        for (VectorI u; u != grid.end_position(); u = grid.next(u)) {
            if (grid(u).a == 0) continue;
            composite(r + u) = grid(u);
        }
    }
    assert(composite.height() % 16 == 0 && composite.width() % 16 == 0);
    for (int y = 0; y < composite.height(); y += 16) {
    for (int x = 0; x < composite.width (); x += 16) {
        // delete all but one base checkboard
        // 544, 128 is THE CHOSEN ONE
        if (x == 544 && y == 128) continue;
        const auto & base = get_base_checkerboard();
        SubGrid<sf::Color> subg(composite, VectorI(x, y), 16, 16);
        bool is_a_checkboard = true;
        for (VectorI r; r != subg.end_position(); r = subg.next(r)) {
            if (base(r) == subg(r)) continue;
            is_a_checkboard = false;
            break;
        }
        if (is_a_checkboard) {
            for (VectorI r; r != subg.end_position(); r = subg.next(r)) {
                subg(r) = sf::Color(0, 0, 0, 0);
            }
        }
    }}
    to_image(composite).saveToFile("/media/ramdisk/atlas.png");
    }

    {
    std::default_random_engine rng { seed };
    to_image(gen_waterfall(16, 14*16, 0, 0)).saveToFile("/media/ramdisk/waterfall0.png");
    to_image(gen_waterfall(16, 14*16, 1, 0)).saveToFile("/media/ramdisk/waterfall1.png");
    to_image(gen_waterfall(16, 14*16, 2, 0)).saveToFile("/media/ramdisk/waterfall2.png");
    to_image(gen_waterfall(16, 14*16, 3, 0)).saveToFile("/media/ramdisk/waterfall3.png");
    }
    for (int i = 0; i != 4*8; ++i) {
        m_textures.emplace_back();
        std::default_random_engine rng { seed };

        m_textures.back().loadFromImage(to_image(
        gen_waterfall(16*10, 14*16, i % 4, i)));
        //add_bottom_ridges( gen_grass_blocks(rng))));
        //gen_cloud(100, 30, 0.005, 4, rng)));
        //to_image(gen_waterfall(8*16, 14*16, i, rng)));//
    }
#   endif
#   if 0
    for (int z = 60; z != 360; ++z) {
        auto grid = gen_shaded_circle(50, sf::Vector3<int>(20, 20, z));
        auto img = to_image(grid);
        sf::Texture texture;
        texture.loadFromImage(img);
        m_textures.push_back(texture);
    }
#   endif
    //m_output.loadFromImage(to_image(gen_grass_blocks(rng)));
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
    return;
    if (m_textures.empty()) return;
    sf::Sprite spt;
    spt.setPosition(22.f*16.f, 19.f*16.f);
    spt.setTexture(m_textures[m_index]);
    target.draw(spt);
}
