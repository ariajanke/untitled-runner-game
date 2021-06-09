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

#include "TreeGraphics.hpp"

#include "BresenhamView.hpp"
#include "FillIterate.hpp"
#include "GraphicsDrawer.hpp"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/RenderTexture.hpp>

#include <common/SfmlVectorTraits.hpp>
#include <common/SubGrid.hpp>

#include <variant>

#include <cassert>

namespace {

using RealDistri = std::uniform_real_distribution<double>;
using IntDistri  = std::uniform_int_distribution<int>;
using Tag        = Spine::Tag;
using Anchor     = Spine::Anchor;
using cul::ConstSubGrid;
using cul::convert_to;
using cul::SubGrid;

template <typename Func>
void for_each_pixel(const Spine &, Func &&);

template <typename Func>
void plot_bresenham_line(VectorI, VectorI, Func &&);

sf::Image to_image(const ConstSubGrid<sf::Color> &);

const sf::Color k_transparent(0, 0, 0, 0);

Grid<sf::Color> generate_leaves(int width, int height, int radius, int count);

} // end of <anonymous> namespace

/* static */ const VectorD PlantTree::k_lean_max_dir = normalize(VectorD(-1, -1));

void PlantTree::plant
    (VectorD location, std::default_random_engine & rng/*, bool go_wide*/)
{
    plant(location, rng, choose_random_leaves_size(rng));
}

void PlantTree::plant(VectorD location, Rng & rng, const RectSize & leaves_size) {
    using Anchor = Spine::Anchor;
    using Tag = Spine::Tag;
#   if 0
    m_seed_value = std::uniform_int_distribution<unsigned>()(rng);
#   endif
    {
    auto h    = RealDistri(k_height_min, k_height_max)(rng);
    auto w    = k_width_min + (k_width_max - k_width_min)*((h - k_height_min) / (k_height_max - k_height_min));
    auto lean = RealDistri(-k_lean_max, k_lean_max)(rng);

    Anchor anc;
    anc.set_location(location)
       .set_width(w).set_pinch((1. - (h - k_height_min) / (k_height_max - k_height_min))*0.25 + 0.75)
       .set_direction(VectorD(0, -1)).set_length(h*0.2);
    Tag tag;
    tag.set_width_angle(k_pi*0.16667)
       .set_location(
            location + VectorD(0, -h*0.5) + rotate_vector(VectorD(0, -h*0.5), lean)
       ).set_direction(normalize(
              VectorD(0, -1)*(1. - lean / k_lean_max)
            + k_lean_max_dir*(lean / k_lean_max)
       ));
    Spine spine;
    spine.set_anchor(anc);
    spine.set_tag   (tag);
    m_trunk_location  = spine.anchor().location();
    m_leaves_location = spine.tag   ().location();

    using IntLims = std::numeric_limits<int>;
    VectorI low (IntLims::max(), IntLims::max());
    VectorI high(IntLims::min(), IntLims::min());
    for_each_pixel(spine, [&low, &high](VectorI r) {
        low .x = std::min(low .x, r.x);
        low .y = std::min(low .y, r.y);
        high.x = std::max(high.x, r.x);
        high.y = std::max(high.y, r.y);
    });
    m_trunk_offset = VectorD( -(m_trunk_location.x - low.x),
                              -(m_trunk_location.y - low.y));
    assert(low.x >= 0 && low.y >= 0);
    static const sf::Color k_color(140, 95, 20);
    Grid<sf::Color> grid;
    grid.set_size(high.x - low.x + 1, high.y - low.y + 1, sf::Color(0, 0, 0, 0));
#   if 0
    const auto & [left, right] = std::make_pair(spine.left_points(), spine.right_points());
    static constexpr const double k_bez_step = 1. / 5.;
    static auto count_bz_itrs = [](const Spine::BezierTuple & tuple) {
        int c = 0;
        for_bezier_lines(tuple, k_bez_step, [&c](VectorD a, VectorD b) {
            for ([[maybe_unused]] auto v : BresenhamView(round_to<int>(a), round_to<int>(b))) ++c;
        });
        return c;
    };
    auto left_count  = count_bz_itrs(left );
    auto right_count = count_bz_itrs(right);
    auto left_pts = make_bezier_array<5>(left);
    auto right_pts = make_bezier_array<5>(left);
#   endif
#   if 1
    for_each_pixel(spine, [&grid, low](VectorI r) {
        grid(r - low) = k_color;
    });

    plot_bresenham_line(
        round_to<int>(std::get<0>(spine.anchor().left_points ())),
        round_to<int>(std::get<0>(spine.anchor().right_points())),
        [&grid, low](VectorI r)
    {
        grid(r - low) = k_color;
    });

    iterate_grid_group(make_sub_grid(grid),
        round_to<int>(spine.anchor().location() + VectorD(0, -2)) - low,
        [&grid](VectorI r) { return grid(r) == sf::Color(0, 0, 0, 0); },
        [&grid](VectorI r, bool) { grid(r) = k_color; });
#   endif
    m_trunk.loadFromImage(to_image(grid));
    }
#   if 0
    auto w = std::round(RealDistri(k_leaves_width_min, k_leaves_width_max)(rng));
    auto h = k_leaves_area / w;
#   endif
    static const auto k_leaves_count = round_to<int>(
        (k_leaves_density*k_leaves_area) / (k_pi*k_leaves_radius*k_leaves_radius));
    auto gen_leaves = [] (int w, int h, sf::Texture & tx) {

        auto img = to_image(generate_leaves(w, h, k_leaves_radius, k_leaves_count));
        tx.loadFromImage(img);
    };

    gen_leaves(leaves_size.width, leaves_size.height, m_fore_leaves);
    gen_leaves(leaves_size.width, leaves_size.height, m_back_leaves);
}

static VectorD trunk_tag_location(VectorD location, const PlantTree::CreationParams & params) {
    auto h    = double(params.trunk_size.height);
    auto lean = double(params.trunk_lean);
    return location + VectorD(0, -h*0.5) + rotate_vector(VectorD(0, -h*0.5), lean);
}

void PlantTree::plant(VectorD location, const CreationParams & params) {
    {
    auto h    = double(params.trunk_size.height);
    auto lean = double(params.trunk_lean);
    Spine::Anchor anc;
    anc.set_location(location)
       .set_width(double(params.trunk_size.width))
       .set_pinch((1. - (h - k_height_min) / (k_height_max - k_height_min))*0.25 + 0.75)
       .set_direction(VectorD(0, -1)).set_length(h*0.2);
    Spine::Tag tag;
    tag.set_width_angle(k_pi*0.16667)
       .set_location(
            //location + VectorD(0, -h*0.5) + rotate_vector(VectorD(0, -h*0.5), lean)
            trunk_tag_location(location, params)
       ).set_direction(normalize(
              VectorD(0, -1)*(1. - lean / k_lean_max)
            + k_lean_max_dir*(lean / k_lean_max)
       ));
    Spine spine;
    spine.set_anchor(anc);
    spine.set_tag   (tag);
    m_trunk_location  = spine.anchor().location();
    m_leaves_location = spine.tag   ().location();

    using IntLims = std::numeric_limits<int>;
    VectorI low (IntLims::max(), IntLims::max());
    VectorI high(IntLims::min(), IntLims::min());
    for_each_pixel(spine, [&low, &high](VectorI r) {
        low .x = std::min(low .x, r.x);
        low .y = std::min(low .y, r.y);
        high.x = std::max(high.x, r.x);
        high.y = std::max(high.y, r.y);
    });
    m_trunk_offset = VectorD( -(m_trunk_location.x - low.x),
                              -(m_trunk_location.y - low.y));
    assert(low.x >= 0 && low.y >= 0);
    static const sf::Color k_color(140, 95, 20);
    Grid<sf::Color> grid;
    grid.set_size(high.x - low.x + 1, high.y - low.y + 1, sf::Color(0, 0, 0, 0));
#   if 0
    const auto & [left, right] = std::make_pair(spine.left_points(), spine.right_points());
    static constexpr const double k_bez_step = 1. / 5.;
    static auto count_bz_itrs = [](const Spine::BezierTuple & tuple) {
        int c = 0;
        for_bezier_lines(tuple, k_bez_step, [&c](VectorD a, VectorD b) {
            for ([[maybe_unused]] auto v : BresenhamView(round_to<int>(a), round_to<int>(b))) ++c;
        });
        return c;
    };
    auto left_count  = count_bz_itrs(left );
    auto right_count = count_bz_itrs(right);
    auto left_pts = make_bezier_array<5>(left);
    auto right_pts = make_bezier_array<5>(left);
#   else
    for_each_pixel(spine, [&grid, low](VectorI r) {
        grid(r - low) = k_color;
    });

    plot_bresenham_line(
        round_to<int>(std::get<0>(spine.anchor().left_points ())),
        round_to<int>(std::get<0>(spine.anchor().right_points())),
        [&grid, low](VectorI r)
    {
        grid(r - low) = k_color;
    });

    iterate_grid_group(make_sub_grid(grid),
        round_to<int>(spine.anchor().location() + VectorD(0, -2)) - low,
        [&grid](VectorI r) { return grid(r) == sf::Color(0, 0, 0, 0); },
        [&grid](VectorI r, bool) { grid(r) = k_color; });
#   endif
    m_trunk.loadFromImage(to_image(grid));
    }

    auto gen_leaves = [&params] (sf::Texture & tx, Grid<bool> * bitmap) {
        const auto & leaves_size = params.leaves_size;
        static const auto k_leaves_count = round_to<int>(
            (k_leaves_density*k_leaves_area) / (k_pi*k_leaves_radius*k_leaves_radius));
        auto leaves = generate_leaves
            (leaves_size.width, leaves_size.height, k_leaves_radius, k_leaves_count);
        tx.loadFromImage(to_image(leaves));
        if (!bitmap) return;
        bitmap->set_size(leaves.width(), leaves.height(), false);
        for (VectorI r; r != leaves.end_position(); r = leaves.next(r)) {
            (*bitmap)(r) = ( leaves(r) != k_transparent );
        }
    };

    gen_leaves(m_fore_leaves, &m_front_leaves_bitmap);
    gen_leaves(m_back_leaves, nullptr);
}

/* static */ PlantTree::CreationParams PlantTree::generate_params(Rng & rng) {
    auto h = RealDistri(k_height_min, k_height_max)(rng);

    CreationParams rv;
    rv.leaves_size = choose_random_leaves_size(rng);
    rv.trunk_size  = RectSize(round_to<int>(
        k_width_min + (k_width_max - k_width_min)*((h - k_height_min) / (k_height_max - k_height_min))),
        round_to<int>(h));
    rv.trunk_lean  = RealDistri(-k_lean_max, k_lean_max)(rng);
    return rv;
}

/* static */ PlantTree::RectSize PlantTree::choose_random_leaves_size
    (std::default_random_engine & rng)
{
    auto w = round_to<int>(RealDistri(k_leaves_width_min, k_leaves_width_max)(rng));
    return RectSize(w, k_leaves_area / w);
}

/* static */ VectorD PlantTree::leaves_location_from_params
    (const CreationParams & params, VectorD plant_location)
{
    VectorD sz_vec( params.leaves_size.width, params.leaves_size.height );
    return trunk_tag_location(plant_location, params) - sz_vec*0.5;
}

void PlantTree::draw(sf::RenderTarget & target, sf::RenderStates states) const {
    render_backs(target, states);
    render_fronts(target, states);
}

void PlantTree::render_fronts(sf::RenderTarget & target, sf::RenderStates states) const {
    sf::Sprite brush;
    brush.setColor(sf::Color(255, 255, 255));
    brush.setTexture(m_fore_leaves);
    brush.setPosition( fore_leaves_location() );
    target.draw(brush, states);
}

void PlantTree::render_backs(sf::RenderTarget & target, sf::RenderStates states) const {
    sf::Sprite brush;
#   if 1
    brush.setColor(sf::Color(180, 180, 180));
    brush.setTexture(m_back_leaves);
    brush.setPosition( fore_leaves_location() + back_leaves_offset() );
    target.draw(brush, states);
#   endif
#   if 1
    sf::Sprite brush2;
    brush2.setColor(sf::Color::White);
    brush2.setTexture(m_trunk);
    brush2.setPosition( trunk_adjusted_location() );
    target.draw(brush2, states);
#   endif
}

void PlantTree::save_to_file(const std::string & fn) const {
    sf::RenderTexture target;
    auto bb = bounding_box();
    if (!target.create(round_to<unsigned>(bb.width), round_to<unsigned>(bb.height))) {
        throw std::runtime_error("");
    }
    sf::View view;
    view.setSize  (float(bb.width), float(bb.height));
    view.setCenter(convert_to<sf::Vector2f>(center_of(bb)));
    target.setView(view);
    target.clear();
    target.draw(*this);
    target.display();
    target.getTexture().copyToImage().saveToFile(fn);
}

Rect PlantTree::bounding_box() const noexcept {
    VectorD tl = convert_to<VectorD>(fore_leaves_location());
    VectorD high(-k_inf, -k_inf);
    auto list = {
        std::make_pair( tl, m_fore_leaves ),
        std::make_pair( tl + convert_to<VectorD>(back_leaves_offset()), m_back_leaves ),
        std::make_pair( convert_to<VectorD>(trunk_adjusted_location()), m_trunk )
    };
    for (const auto & [pos, texture] : list) {
        high.x = std::max(high.x, pos.x + double(texture.getSize().x));
        high.y = std::max(high.y, pos.y + double(texture.getSize().y));
    }
    return Rect( tl, convert_to<cul::Size2<double>>(high - tl) );
}


/* private */ sf::Vector2f PlantTree::fore_leaves_location() const noexcept {
    return convert_to<sf::Vector2f>(m_leaves_location) - sf::Vector2f( m_fore_leaves.getSize() )*0.5f;
}

/* private */ sf::Vector2f PlantTree::trunk_adjusted_location() const noexcept {
    return convert_to<sf::Vector2f>( m_trunk_location + m_trunk_offset );
}

/* private */ sf::Vector2f PlantTree::back_leaves_offset() const noexcept {
    return sf::Vector2f(15, 15);
}

std::tuple<VectorD, VectorD> Tag::left_points() const {
    return points(-m_width*0.5);
}

std::tuple<VectorD, VectorD> Tag::right_points() const {
    return points(m_width*0.5);
}

Tag & Tag::set_location(VectorD r) {
    assert(is_real(r.x) && is_real(r.y));
    m_location = r;
    return *this;
}

Tag & Tag::set_direction(VectorD r) {
    assert(is_real(r.x) && is_real(r.y));
    assert(!are_very_close(r, VectorD()));
    assert(are_very_close(magnitude(r), 1.));
    m_direction = to_direction(r);
    return *this;
}

Tag & Tag::set_width_angle(double x) {
    assert(is_real(x));
    assert(x >= 0.);
    m_width = x;
    return *this;
}

VectorD Tag::location() const {
    return m_location;
}

VectorD Tag::direction() const {
    return to_unit_circle_vector(m_direction);
}

/* private */ std::tuple<VectorD, VectorD> Tag::points(double from_center) const {
    return std::make_tuple(
        m_location + 20.*to_unit_circle_vector(m_direction - from_center),
        m_location);
}

Anchor & Anchor::set_location(VectorD r) {
    assert(is_real(r.x) && is_real(r.y));
    m_location = r;
    return *this;
}

Anchor & Anchor::set_berth(double) {
    return *this;
}

Anchor & Anchor::set_width(double w) {
    assert(is_real(w));
    m_width = w;
    return *this;
}

Anchor & Anchor::set_pinch(double x) {
    assert(is_real(x));
    assert(x >= 0. && x <= 1.);
    m_pinch = x;
    return *this;
}

Anchor & Anchor::set_length(double l) {
    assert(is_real(l));
    m_length = l;
    return *this;
}

Anchor & Anchor::set_direction(VectorD r) {
    assert(is_real(r.x) && is_real(r.y));
    assert(!are_very_close(r, VectorD()));
    assert(are_very_close(magnitude(r), 1.));
    m_direction = to_direction(r);
    return *this;
}

void Anchor::sway_toward(const Tag &) {

}

VectorD Anchor::location() const {
    return m_location;
}

VectorD Anchor::direction() const {
    return to_unit_circle_vector(m_direction);
}

/* private */ std::tuple<VectorD, VectorD> Anchor::get_points(double offset) const {
    auto offsetv = to_unit_circle_vector(m_direction - k_pi*0.5)*offset;
    auto to_b = rotate_vector(to_unit_circle_vector(m_direction)*m_length,
                              normalize(offset)*k_pi*(1. / 6.));
    return std::make_tuple(m_location + offsetv,
                           m_location + offsetv*m_pinch + to_b);
}

namespace {

class SineDistribution {
public:
    SineDistribution(double min, double max): m_min(min), m_max(max) {}

    template <typename Urng>
    double operator () (Urng & rng) const {
        auto x = std::uniform_real_distribution<double>(k_error, 1. - k_error)(rng);
        return m_min + (m_max - m_min)*(std::acos(1 - x) / k_pi);
    }
private:
    double m_min, m_max;
};

namespace tree_bundle_classes {

enum Enum {
    k_air          =     0,
    k_unclassified =  0b01,
    k_island       =  0b10,
    k_body         =  0b11,
};

}

class BundleClass {
public:
    BundleClass(): m_class(tree_bundle_classes::k_air), m_hull(false) {}

    bool is_air   () const noexcept { return m_class == tree_bundle_classes::k_air; }
    bool is_bundle() const noexcept { return m_class != tree_bundle_classes::k_air; }
    bool is_hull  () const noexcept { return m_hull; }

    void mark_as_hull() {
        if (!is_bundle()) throw std::runtime_error("");
        m_hull = true;
    }

    void classify_as(tree_bundle_classes::Enum clss) { m_class = clss; }

    tree_bundle_classes::Enum get_class() const noexcept
        { return tree_bundle_classes::Enum(m_class); }

private:
    uint8_t m_class : 2;
    bool m_hull : 1;
};

using BezierTriple = std::tuple<VectorD, VectorD, VectorD>;

const std::array k_foilage_web_pallete = {
    sf::Color(20, 230, 20), sf::Color(10, 200, 10), sf::Color(20, 180, 20),
    sf::Color(0, 150, 0, 0),
    sf::Color(20, 230, 20), sf::Color(10, 200, 10), sf::Color(20, 180, 20),
    sf::Color(0, 150, 0, 0),
    sf::Color(10, 200, 10),
};

const std::array k_leaf_bundle_pallete = {
    sf::Color(20, 230, 20), sf::Color(10, 200, 10), sf::Color(20, 180, 20),
};

constexpr const int k_wood_texture_width  = 100;
constexpr const int k_wood_texture_height = k_wood_texture_width;

Grid<sf::Color> make_wood_texture();
const Grid<sf::Color> k_wood_texture = make_wood_texture();
#if 0
[] {
    constexpr const int k_partitions = 5;

    std::default_random_engine rng { std::random_device()() };
    Grid<sf::Color> rv;
    rv.set_size(k_wood_texture_width, k_wood_texture_height);

    return Grid<sf::Color>();
} ();
#endif

template <typename T, std::size_t k_count>
std::pair<const T *, const T *> make_view_pair(const std::array<T, k_count> & array)
    { return std::make_pair(&array.front(), &array.front() + array.size()); }

//constexpr const int k_max_width  = 512;
//constexpr const int k_max_height = k_max_width;

template <typename Urng, typename Func>
void for_ellip_distri(const Rect & bounds, int times_done, Urng &, Func &&);

bool is_within_circle(VectorI center, int radius, VectorI r);

template <typename Func>
void for_each_point_in_circle(VectorI center, int radius, Func &&);

void draw_disk(SubGrid<sf::Color>, VectorI r, int radius, sf::Color color);

void classify_bundles(const std::vector<VectorI> & bundle_points, int radius, SubGrid<BundleClass>, VectorI body_root);

std::vector<VectorI> find_convex_hull(const std::vector<VectorI> &);

std::vector<BezierTriple> make_triples
    (const std::vector<VectorI> & hull_points, ConstSubGrid<BundleClass>, int radius);

VectorD find_hull_center_without_sunken
    (const std::vector<BezierTriple> &, const std::vector<VectorI> & hull_points_vec);

std::vector<BezierTriple> make_front_curves
    (const std::vector<BezierTriple> &, VectorD adjusted_hull_center, VectorD root_pos);

Grid<bool> make_foilage_web_mask(int width, int height, const std::vector<BezierTriple> &);

Grid<sf::Color> make_builtin_leaf_texture(std::pair<const sf::Color *, const sf::Color *>);

template <typename Func>
void for_each_pixel(const Spine & spine, Func && f) {
    using BezierTuple = Spine::BezierTuple;
    spine.render_to(make_spine_renderer(
        [&f](const BezierTuple & left, const BezierTuple & right)
    {
        for (const auto & bz_pts : { left, right }) {
        for_bezier_lines(bz_pts, 1. / 20., [&f](VectorD a, VectorD b) {
            plot_bresenham_line(round_to<int>(a), round_to<int>(b), f);
        }); }
    }));
}
#if 0
// More information about this algorithm can be found here:
// https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
class BresenhamIterator {
public:
    enum { k_jump_to_end };

    BresenhamIterator() {}

    BresenhamIterator(VectorI beg, VectorI last):
        m_pos   (beg),
        m_delta ( magnitude(last.x - beg.x), -magnitude(last.y - beg.y) ),
        m_step  ( norm(last.x - beg.x), norm(last.y - beg.y) ),
        m_error ( m_delta.x + m_delta.y )
        //m_step_y( norm(last.y - beg.y) )
    {}

    BresenhamIterator(VectorI beg, VectorI last, decltype(k_jump_to_end)):
        BresenhamIterator(beg, last)
    {
        m_pos = last;
        advance();
    }

    BresenhamIterator & operator ++ () {
        advance();
        return *this;
    }

    BresenhamIterator operator ++ (int) {
        auto t = *this;
        advance();
        return t;
    }

    VectorI operator * () const noexcept { return m_pos; }

    bool operator == (const BresenhamIterator & rhs) const noexcept
        { return is_same(rhs); }

    bool operator != (const BresenhamIterator & rhs) const noexcept
        { return !is_same(rhs); }

private:
    void advance() {
        int e2 = m_error*2;
        if (e2 >= m_delta.y) {
            m_error += m_delta.y;
            m_pos.x += step().x;
        }
        if (e2 <= m_delta.x) {
            m_error += m_delta.x;
            m_pos.y += step().y;
        }
    }

    bool is_same(const BresenhamIterator & rhs) const noexcept
        { return m_pos == rhs.m_pos; }

    VectorI step() const noexcept
        { return m_step; }// VectorI(norm(m_delta.x), m_step_y); }

    static int norm(int x) { return x <= 0 ? -1 : 1; }

    VectorI m_pos, m_delta, m_step;
    int m_error = 0;//, m_step_y = 0;
};

class BresenhamView {
public:
    BresenhamView(VectorI beg, VectorI last): m_beg(beg), m_last(last) {}

    BresenhamIterator begin() const
        { return BresenhamIterator(m_beg, m_last); }

    BresenhamIterator end  () const
        { return BresenhamIterator(m_beg, m_last, BresenhamIterator::k_jump_to_end); }

private:
    VectorI m_beg, m_last;
};
#endif
template <typename Func>
void plot_bresenham_line(VectorI a, VectorI b, Func && f) {
    for (auto v : BresenhamView(a, b)) f(v);
#   if 0
    // More information about this algorithm can be found here:
    // https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
    static auto norm = [](int x) { return x <= 0 ? -1 : 1; };
    auto dx  =  magnitude(b.x - a.x);
    auto sx  =  norm     (b.x - a.x);
    auto dy  = -magnitude(b.y - a.y);
    auto sy  =  norm     (b.y - a.y);
    auto err = dx + dy;
    while (true) {
        f(a);
        if (a == b) return;
        auto e2 = 2*err;
        if (e2 >= dy) {
            err += dy;
            a.x += sx;
        }
        if (e2 <= dx) {
            err += dx;
            a.y += sy;
        }
    }
#   endif
}

sf::Image to_image(const ConstSubGrid<sf::Color> & cgrid) {
    sf::Image img;
    img.create(unsigned(cgrid.width()), unsigned(cgrid.height()));
    for (VectorI r; r != cgrid.end_position(); r = cgrid.next(r)) {
        img.setPixel(unsigned(r.x), unsigned(r.y), cgrid(r));
    }
    return img;
}

Grid<sf::Color> generate_leaves(int width, int height, int radius, int count) {
    static const auto k_foilage_texture = make_builtin_leaf_texture(make_view_pair(k_foilage_web_pallete));
    static const auto k_bundle_texture  = make_builtin_leaf_texture(make_view_pair(k_leaf_bundle_pallete));

    static const bool k_init_me = [] {
        to_image(k_foilage_texture).saveToFile("/media/ramdisk/foilage-texture.png");
        to_image(k_bundle_texture).saveToFile("/media/ramdisk/bundle-texture.png");
        return true;
    } ();

    std::default_random_engine rng { std::random_device()() };
    std::vector<VectorI> bundle_points;
    bundle_points.reserve(count);
    for_ellip_distri(
        Rect(radius, radius, width - radius*2, height - radius*2),
        count, rng, [&bundle_points](VectorD r)
    {
        bundle_points.push_back(round_to<int>(r));
    });

    Grid<BundleClass> class_grid;
    class_grid.set_size(width, height);
    classify_bundles(bundle_points, radius, class_grid, VectorI(width / 2, height / 2));
    auto hull_points = find_convex_hull(bundle_points);
    for (const auto & v : hull_points) {
        class_grid(v).mark_as_hull();
    }
    auto bezier_triples = make_triples(hull_points, class_grid, radius);
    auto adjusted_hull_center = find_hull_center_without_sunken(bezier_triples, hull_points);
    auto front_curves = make_front_curves(bezier_triples, adjusted_hull_center, VectorD(width / 2, height / 2));
    auto mask = make_foilage_web_mask(width, height, front_curves);
    Grid<sf::Color> samp;
    samp.set_size(width, height, k_transparent);

    for (VectorI r; r != samp.end_position(); r = samp.next(r)) {
        if (mask(r)) samp(r) = k_foilage_texture(r);
    }
    for (const auto & v : hull_points) {
        //if (class_grid(v).get_class() == tree_bundle_classes::k_body) continue;
        for_each_point_in_circle(v, radius, [&samp](VectorI r) {
            samp(r) = k_bundle_texture(r);
        });
    }
    for (const auto & v : bundle_points) {
        if (class_grid(v).get_class() != tree_bundle_classes::k_island) continue;
        plot_bresenham_line(v, VectorI(width /2, height /2), [&samp](VectorI r) { samp(r) = sf::Color(180, 140, 10); });
    }
    return samp;
}

// ----------------------------------------------------------------------------

const auto k_no_location = k_no_intersection;

template <bool k_is_const, typename T>
using SelectVecIterator =
    std::conditional_t<k_is_const, typename std::vector<T>::const_iterator, typename std::vector<T>::iterator>;

template <bool k_is_const, typename T>
using SelectStdVectorRef =
    std::conditional_t<k_is_const, const typename std::vector<T> &, typename std::vector<T> &>;

using BezierTripleIter = std::vector<BezierTriple>::const_iterator;

class GwPoints {
public:
    static constexpr const std::size_t k_make_copy_thershold = 1024;

    class Interface {
    public:
        using ConstVecPtr = const VectorI *;
        virtual ~Interface() {}
        virtual VectorI get_init_v() = 0;
        virtual void mark_done_with(ConstVecPtr &) = 0;
        virtual const std::vector<VectorI> & points() const = 0;
    };

    class WritablePts final : public Interface {
    public:
        WritablePts(const std::vector<VectorI> & copy_): m_copy(copy_) {}
    private:
        VectorI get_init_v() override;

        void mark_done_with(ConstVecPtr &) override;

        const std::vector<VectorI> & points() const override { return m_copy; }

        std::vector<VectorI> m_copy;
    };

    class ReadOnlyPts final : public Interface {
    public:
        ReadOnlyPts(const std::vector<VectorI> & cref): m_ref(cref) {}
    private:
        VectorI get_init_v() override
            { return get_extremes(m_ref).front(); }
        void mark_done_with(ConstVecPtr & p) override { p = nullptr; }
        const std::vector<VectorI> & points() const override { return m_ref; }
        const std::vector<VectorI> & m_ref;
    };

    using PtsVari = std::variant<WritablePts, ReadOnlyPts>;

    static PtsVari make_pt_handler(const std::vector<VectorI> & vec);

    static Interface * get_interface(PtsVari & var);

private:
    friend class WritablePts;
    friend class ReadOnlyPts;
    // lowx is always first element
    static std::array<VectorI, 4> get_extremes(const std::vector<VectorI> &);
};

std::vector<VectorI> find_convex_hull(GwPoints::Interface &);

template <typename T, typename IterType>
cul::Vector2<T> find_center(IterType beg, IterType end);

template <bool k_is_const, typename T>
std::vector<SelectVecIterator<k_is_const, cul::Vector2<T>>>
    find_opposite_sides(SelectStdVectorRef<k_is_const, cul::Vector2<T>> cont,
                        const cul::Vector2<T> & hull_center);

template <typename Iter, typename Func>
void for_side_by_side_wrap(Iter beg, Iter end, Func &&);

// gets denormalized vector jutting out of the hull at a given segment
// normalizing this vector will get the normal of this hull triangle/segment
template <typename T>
cul::Vector2<T> get_hull_out_of_segment
    (const cul::Vector2<T> & a, const cul::Vector2<T> & b,
     const cul::Vector2<T> & hull_center);

template <typename OnHolesFunc, typename OnFillsFunc>
void for_each_holes_and_fills
    (BezierTripleIter beg, BezierTripleIter end, OnHolesFunc &&, OnFillsFunc &&);

template <typename Urng, typename Func>
void for_ellip_distri(const Rect & bounds, int times_done, Urng & rng, Func && f) {
    using RealDistri = std::uniform_real_distribution<double>;
    VectorD origin = center_of(bounds);
    //double ray_length = std::max(bounds.width, bounds.height)*1.1;
    auto hrad = bounds.width *0.5;
    auto vrad = bounds.height*0.5;
    for (int i = 0; i != times_done; ++i) {
        auto dir = to_unit_circle_vector(RealDistri(0, k_pi*2.)(rng));
        //auto intx = find_intersection(bounds, origin, origin + dir*ray_length);
        //auto gv = magnitude(find_intersection(bounds, origin, origin + dir*ray_length) - origin);
        auto r = hrad*vrad / std::sqrt(  vrad*vrad*dir.x*dir.x
                                       + hrad*hrad*dir.y*dir.y  );
        assert(is_real(r));
        auto mag = SineDistribution(-r, r)(rng);
        f(origin + dir*mag);
    }
}

Grid<sf::Color> make_wood_texture() {
    //constexpr const int k_partitions = 5;

    static const sf::Color k_base(0x40, 0x37, 0x0F);
    static const sf::Color k_bump(0x61, 0x45, 0x11);

    // I need furrow colors
    std::default_random_engine rng { std::random_device()() };
    Grid<sf::Color> rv;
    rv.set_size(k_wood_texture_width, k_wood_texture_height, k_base);

    VectorI line(5, 7);
    for (int y = line.y; y + line.y < k_wood_texture_height; y += line.y) {
    for (int x = 0; x < y; x += 3) {
        VectorI r(x, y);
        for (auto v : BresenhamView(r, r + line)) {
            rv(v) = k_bump;
        }
    }}
    to_image(rv).saveToFile("/media/ramdisk/trunk-texture.png");
    return rv;
}

bool is_within_circle(VectorI center, int radius, VectorI r) {
    auto diff = r - center;
    return diff.x*diff.x + diff.y*diff.y <= radius*radius;
}

template <typename Func>
void for_each_point_in_circle(VectorI center, int radius, Func && f) {
    using namespace cul::fc_signal;
    VectorI v = center - VectorI(radius, radius);
    auto x_start = v.x;
    auto x_end = v.x + radius*2;
    auto y_end = v.y + radius*2;
    while (v.y != y_end) {
        if (is_within_circle(center, radius, v)) {
            auto gv = adapt_to_flow_control_signal(f, v);
            if (gv == k_break) return;
        }
        if (++v.x == x_end) {
            v.x = x_start;
            ++v.y;
        }
    }
}

void draw_disk(SubGrid<sf::Color> target, VectorI r, int radius, sf::Color color) {
#   if 0
    const VectorI k_center(radius, radius);
#   endif
    for_each_point_in_circle(r, radius, [&target, color](VectorI r) {
        auto c2 = color;
        if (color.a != 255) c2.a = ((r.x + r.y / 4) % 2) ? 255 : color.a;
        target(r) = c2;
    });
#   if 0
    auto subg = make_sub_grid(target, r - VectorI(1, 1)*radius, radius*2, radius*2);
    for (VectorI v; v != subg.end_position(); v = subg.next(v)) {
        if (!is_within_circle( k_center, radius, v )) continue;
        auto c2 = color;
        if (color.a != 255) c2.a = ((v.x + v.y / 4) % 2) ? 255 : color.a;
        subg(v) = c2;
    }
#   endif
}

void classify_bundles
    (const std::vector<VectorI> & bundle_points, int radius,
     SubGrid<BundleClass> classgrid, VectorI body_root)
{
    for (const auto & pt : bundle_points) {
        auto cgrid = make_sub_grid(classgrid, pt - VectorI(1, 1)*radius, radius*2, radius*2);
        for (VectorI r; r != cgrid.end_position(); r = cgrid.next(r)) {
            if (r.x*r.x + r.y*r.y < radius*radius) continue;
            cgrid(r).classify_as(tree_bundle_classes::k_unclassified);
        }
    }
    iterate_grid_group(classgrid, body_root,
                       [&classgrid](VectorI r) { return !classgrid(r).is_air(); },
                       [&classgrid](VectorI r, bool) { classgrid(r).classify_as(tree_bundle_classes::k_body); });
    for (auto & bc : classgrid) {
        if (bc.get_class() == tree_bundle_classes::k_unclassified)
            bc.classify_as(tree_bundle_classes::k_island);
    }
}

std::vector<VectorI> find_convex_hull(const std::vector<VectorI> & pts) {
    auto varlivingspace = GwPoints::make_pt_handler(pts);
    return find_convex_hull(*GwPoints::get_interface(varlivingspace));
}

std::vector<BezierTriple> make_triples
    (const std::vector<VectorI> & hull_points_vec,
     ConstSubGrid<BundleClass> grid_class, int radius)
{
    auto hull_center = find_center<int>(hull_points_vec.begin(), hull_points_vec.end());
    auto opposites = find_opposite_sides<true>(hull_points_vec, hull_center);
    // dips are the middle points
    std::vector<BezierTriple> bezier_triples;
    bezier_triples.reserve(hull_points_vec.size());
    for_side_by_side_wrap(hull_points_vec.begin(), hull_points_vec.end(),
        [&](const VectorI & a, const VectorI & b)
    {
        auto para = normalize(get_hull_out_of_segment(VectorD(a), VectorD(b), VectorD(hull_center)));
        auto mid = (VectorD(a) + VectorD(b))*0.5 + para;
        // step until we hit an occupied pixel or we cross the "opposite" side
        VectorD dip_pos = k_no_location;
        VectorD step;
        double stop_distance;
        {
        auto itr = *(opposites.begin() + (&a - &hull_points_vec.front()));
        VectorD stop_a { *itr };
        VectorD stop_b { itr + 1 == hull_points_vec.end() ? *hull_points_vec.begin() : *(itr + 1) };
        auto to_stop = ( (stop_a + stop_b)*0.5 ) - mid;
        assert( find_intersection(stop_a, stop_b, mid, mid + to_stop*1.1) != k_no_intersection );
        step = normalize(to_stop);
        stop_distance = magnitude(to_stop);
        }
        for (auto pos = mid + step; magnitude(pos - mid) < stop_distance; pos += step)
        {
            if (!grid_class(round_to<int>(pos)).is_air()) {
                dip_pos = pos;
                break;
            }
        }
        bezier_triples.emplace_back(
            VectorD(a) + para*double(radius),
            dip_pos,
            VectorD(b) + para*double(radius));
    });
    assert(bezier_triples.size() == hull_points_vec.size());
    return bezier_triples;
}

VectorD find_hull_center_without_sunken
    (const std::vector<BezierTriple> & bezier_triples, const std::vector<VectorI> & hull_points_vec)
{
    VectorD nhull_center = k_no_location;
    auto avg_taker = [] {
        int i = 0;
        return [i] (VectorD & avg, VectorD r) mutable {
            if (i++ == 0) {
                avg = r;
                return;
            }
            auto di = double(i);
            avg = r*( 1. / di ) + avg*( double( i - 1 ) / di );
        };
    } ();
    auto inc_hull_points_idx = [&hull_points_vec] (int i) {
        assert(i >= 0 && i < int(hull_points_vec.size()));
        return (i + 1) % int(hull_points_vec.size());
    };
    auto get_hull_points = [&hull_points_vec, &inc_hull_points_idx](int i) {
        assert(i >= 0 && i < int(hull_points_vec.size()));
        auto hp_f = hull_points_vec[i];
        auto hp_s = hull_points_vec[inc_hull_points_idx(i)];
        return std::make_tuple(hp_f, hp_s);
    };
    for_each_holes_and_fills(
        bezier_triples.begin(), bezier_triples.end(),
        [](BezierTripleIter, BezierTripleIter, int) { /* skip holes */ },
        [&avg_taker, &nhull_center, &inc_hull_points_idx, &get_hull_points]
        (BezierTripleIter beg, BezierTripleIter end, int idx)
    {
        for (auto itr = beg; itr != end; ++itr) {
            avg_taker(nhull_center, VectorD( std::get<0>(get_hull_points(idx)) ));
            idx = inc_hull_points_idx(idx);
        }
        if (beg != end) {
            avg_taker(nhull_center, VectorD( std::get<1>(get_hull_points(idx))));
        }
    });
    return nhull_center;
}

std::vector<BezierTriple> make_front_curves
    (const std::vector<BezierTriple> & bz_triples,
     VectorD adjusted_hull_center, VectorD root_pos)
{
    using std::get;
    VectorD ex_center = (adjusted_hull_center + root_pos)*0.5;
    double ex_radius = std::max(magnitude(adjusted_hull_center - root_pos), 1.);
    // note: no points should be in the exclusion zone, defined as:
    // smallest possible circular region enclosing the adjusted_hull_center AND root pos

    // how do I know that the fill is enclosing the exclusion zone too?
    // cross product perhaps?
    static auto triple_is_real = [](const BezierTriple & triple) {
        return    is_real(get<0>(triple).x) && is_real(get<0>(triple).y)
               && is_real(get<1>(triple).x) && is_real(get<1>(triple).y)
               && is_real(get<2>(triple).x) && is_real(get<2>(triple).y);
    };
    std::vector<BezierTriple> triples;
    triples.reserve(bz_triples.size());
    // patch 1 sized holes with "new hull center"
    for_each_holes_and_fills(
        bz_triples.begin(), bz_triples.end(),
        [&triples, &bz_triples](BezierTripleIter beg, BezierTripleIter end, int idx) {
            if (end - beg == 1) {
                int prev_idx = idx - 1 < 0 ? int(bz_triples.size() - 1) : idx - 1;
                int next_idx = idx + 1 >= int(bz_triples.size()) ? 0 : idx + 1;
                auto avg = 0.5*(get<1>( bz_triples.at(prev_idx) ) +
                                get<1>( bz_triples.at(next_idx) ));
                auto trip = bz_triples[idx];
                get<1>(trip) = avg;
                //get<1>( triples[idx] ) = avg;
                triples.emplace_back(trip);
                assert(triple_is_real(triples.back()));
            } else {
                // multiples are way harder
                auto med = beg + ((end - beg) / 2);
                triples.emplace_back(get<0>(*(end - 1)), get<0>(*med), get<2>(*beg));
                assert(triple_is_real(triples.back()));
            }
        },
        [&triples] (BezierTripleIter beg, BezierTripleIter end, int)
    {
        triples.insert(triples.begin(), beg, end);
    });
    for (const auto & trip : triples)
        assert(triple_is_real(trip));
    return triples;
}

Grid<bool> make_foilage_web_mask
    (int width, int height, const std::vector<BezierTriple> & triples)
{
    using std::get;
    Grid<bool> mold;
    mold.set_size(width, height, false);
    if (triples.empty()) return mold;
    auto do_line = [&mold](const VectorD a, VectorD b) {
        plot_bresenham_line(round_to<int>(a), round_to<int>(b), [&mold](VectorI r) { mold(r) = true; });
    };
    auto do_bezier = [&do_line](const BezierTriple & a) {
        for_bezier_lines(a, 1. / 5., do_line);
    };
    for_side_by_side_wrap(triples.begin(), triples.end(),
        [&do_bezier, &do_line](const BezierTriple & a, const BezierTriple & b)
    {
        auto bez_a = std::make_tuple(
            get<0>(a),
            (get<0>(a) + get<2>(a))*0.5,
            get<1>(a));
        auto bez_b = std::make_tuple(
            get<1>(a),
            (get<0>(a) + get<2>(a))*0.5,
            get<2>(a));
        //do_bezier(a);
        do_bezier(bez_a);
        do_bezier(bez_b);
        do_line(get<2>(a), get<0>(b));
    });
    Grid<bool> mask;
    mask.set_size(width, height, false);
    iterate_grid_group(
        make_sub_grid(mold), VectorI(width / 2, height / 2),
        [&mold](VectorI r) { return !mold(r); },
        [&mask](VectorI r, bool) { mask(r) = true; });
    return mask;
}

Grid<sf::Color> make_builtin_leaf_texture(std::pair<const sf::Color *, const sf::Color *> view_pair) {
    static constexpr const int k_max_width  = 512;
    static constexpr const int k_max_height = k_max_width;
    std::default_random_engine rng { std::random_device()() };
    static constexpr const int k_radius = 4;
    Grid<sf::Color> rv;
    rv.set_size(k_max_width, k_max_height);
    static constexpr const int k_min_delta = k_radius*2 - 2;
    static constexpr const int k_max_delta = k_radius*2 - 1;
    static_assert(k_min_delta > 0, "");

    auto delta_distri = std::uniform_int_distribution<int>(k_min_delta, k_max_delta);
    int i = 0;
    for (int y = k_max_height - k_radius; y > k_radius; y -= delta_distri(rng) / 2) {
        for (int x = k_radius; x < k_max_width - k_radius; x += delta_distri(rng)) {
            auto c = *(view_pair.first + ((++i) % (view_pair.second - view_pair.first)));
            draw_disk(rv, VectorI(x, y), k_radius, c);
        }
    }
    return rv;
}

// ----------------------------------------------------------------------------

using PtsVari = GwPoints::PtsVari;

template <typename IterType, typename T>
bool contains_point
    (IterType beg, IterType end,
     const cul::Vector2<T> & center, const cul::Vector2<T> & pt);

VectorI GwPoints::WritablePts::get_init_v() {
    auto gv = get_extremes(m_copy);
    auto rv = gv.front();
    // remove predicate
    auto end = std::unique(gv.begin(), gv.end(), [](VectorI a, VectorI b) { return a == b; });

    auto c = find_center<int>(gv.begin(), end);

    auto pt_not_cont = [&gv, end, c](VectorI r) {
        return !contains_point(gv.begin(), end, c, r);
    };
    m_copy.erase(std::remove_if(m_copy.begin(), m_copy.end(), pt_not_cont),
                 m_copy.end());
    return rv;
}

void GwPoints::WritablePts::mark_done_with(ConstVecPtr & p) {
    assert(!m_copy.empty());
    assert(p > &m_copy.front() && p < &m_copy.back());
    auto itr = (p - &m_copy.front()) + m_copy.begin();
    if (itr != m_copy.end())
        std::swap(*itr, *(m_copy.end() - 1));
    m_copy.pop_back();
    p = nullptr;
}

/* static */ PtsVari GwPoints::make_pt_handler(const std::vector<VectorI> & vec) {
    if (vec.size() >= k_make_copy_thershold) return PtsVari { WritablePts(vec) };
    return PtsVari { ReadOnlyPts(vec) };
}

/* static */ GwPoints::Interface * GwPoints::get_interface(PtsVari & var) {
    struct Vis {
        Vis(Interface ** rv_): rv(rv_) {}
        void operator () (WritablePts & r) const { *rv = &r; }
        void operator () (ReadOnlyPts & r) const { *rv = &r; }
        Interface ** rv = nullptr;
    };
    Interface * rv = nullptr;
    std::visit(Vis(&rv), var);
    return rv;
}

/* private static */ std::array<VectorI, 4> GwPoints::get_extremes
    (const std::vector<VectorI> & vec)
{
    using Lims = std::numeric_limits<int>;
    VectorI lowx(Lims::max(), 0), highx(Lims::min(), 0);
    VectorI lowy(0, Lims::max()), highy(0, Lims::min());
    for (const auto & r : vec) {
        if (lowx.x  > r.x || (lowx.x == r.x && lowx.y > r.y)) lowx = r;
        if (lowy.y  > r.y) lowy  = r;
        if (highx.x < r.x) highx = r;
        if (highy.y < r.y) highy = r;
    }
    return { lowx, lowy, highx, highy };
}

std::vector<VectorI> find_convex_hull(GwPoints::Interface & pts_intf) {
    if (pts_intf.points().size() < 3) return std::vector<VectorI>();

    // "current" vector being processed
    VectorI v = pts_intf.get_init_v();

    static const VectorI k_ex_left_start_dir(-1, 0);
    static const double  k_rot_dir = [] {
        static const VectorI k_dir_prev(0, 1);
        auto gv = rotate_vector(VectorD(k_dir_prev), k_pi*0.5);
        if (round_to<int>(gv) == k_ex_left_start_dir) return 1.;
        return -1.;
    } ();

    std::vector<VectorI> rv;
    rv.push_back(v);

    VectorD vn { k_ex_left_start_dir };
    do {
        VectorD cand_n = vn;
        const VectorI * cand_v = nullptr;
        double cand_rot = k_pi*2.;
        // need to handle colinears
        // so we'll favor the furthest point in our search
        double cand_dist = 0.;
        for (const auto & pt : pts_intf.points()) {
            // ignore other equal points, our candidate hull already "contains"
            // the point
            if (v == pt) continue;
            // definitely NOT the zero vector
            auto to_v = VectorD(v - pt);

            VectorD cons_n = normalize(rotate_vector(to_v, k_pi*0.5*k_rot_dir));
            double cons_dist = magnitude(VectorD(v - pt));
            auto cons_rot = angle_between(vn, cons_n);
            if (!are_very_close( rotate_vector(vn, cons_rot*k_rot_dir), cons_n))
                cons_rot = k_pi*2 - cons_rot;
            assert(are_very_close(rotate_vector(vn, cons_rot*k_rot_dir), cons_n));
            if (are_very_close(cons_rot, cand_rot)) {
                if (cons_dist <= cand_dist)
                    continue;
            } else if (cons_rot > cand_rot) {
                continue;
            }
            // new candidate
            cand_v    = &pt;
            cand_n    = cons_n;
            cand_rot  = cons_rot;
            cand_dist = cons_dist;
        }
        // post search
        // cand_v needs to not be v
        assert(cand_v);
        rv.push_back(v = *cand_v);
        // every point maybe in the hull
        assert(rv.size() <= pts_intf.points().size() + 1);
        pts_intf.mark_done_with(cand_v);
        vn = cand_n;
    } while (rv.front() != v);
    rv.pop_back();

    return rv;
}

template <typename T, typename IterType>
cul::Vector2<T> find_center(IterType beg, IterType end) {
    // testing for overflow will be a fun test case
    cul::Vector2<T> avg;
    for (auto itr = beg; itr != end; ++itr) {
        avg += *itr;
    }
    avg.x /= (end - beg);
    avg.y /= (end - beg);
    return avg;
}

template <bool k_is_const, typename T>
std::vector<SelectVecIterator<k_is_const, cul::Vector2<T>>>
    find_opposite_sides(SelectStdVectorRef<k_is_const, cul::Vector2<T>> cont,
                        const cul::Vector2<T> & hull_center)
{
    using Vec = cul::Vector2<T>;
    std::vector<SelectVecIterator<k_is_const, Vec>> rv;
    static constexpr const double k_init_ray_length = 1000.;
    rv.reserve(cont.size());

    for_side_by_side_wrap(cont.begin(), cont.end(), [&](const Vec & a, const Vec & b) {
        auto norm = cul::normalize(get_hull_out_of_segment(VectorD(a), VectorD(b), VectorD(hull_center)));
        auto mid  = (VectorD(a) + VectorD(b))*0.5;
        auto ray_length = k_init_ray_length;

        const Vec * cand = nullptr;
        while (!cand) {
            if (!is_real(ray_length)) {
                throw std::invalid_argument("Cannot find opposite of a segment");
            }

            for_side_by_side_wrap(cont.begin(), cont.end(), [&](const Vec & ap, const Vec & bp) {
                using namespace cul::fc_signal;
                if (&a == &ap) return k_continue;
                auto intx = find_intersection(mid, mid + -ray_length*norm, VectorD(ap), VectorD(bp));
                if (intx == k_no_intersection) return k_continue;
                cand = &ap;
                return k_break;
            });
            ray_length *= 2.;
        }
        rv.push_back( cont.begin() + (cand - &cont.front()) );
    });
    return rv;
}

template <typename Iter, typename Func>
void for_side_by_side_wrap(Iter beg, Iter end, Func && f) {
    using namespace cul::fc_signal;
    if (end - beg < 2) return;
    for (auto itr = beg; itr != end - 1; ++itr) {
        const auto & a = *itr;
        const auto & b = *(itr + 1);
        auto fc = adapt_to_flow_control_signal(f, a, b);
        if (fc == k_break) return;
    }
    const auto & a = *(end - 1);
    const auto & b = *beg;
    f(a, b);
}

template <typename T>
cul::Vector2<T> get_hull_out_of_segment
    (const cul::Vector2<T> & a, const cul::Vector2<T> & b,
     const cul::Vector2<T> & hull_center)
{
    using Vec = cul::Vector2<T>;
    Vec mid { (a + b) / T(2) };
    Vec from = mid - Vec(hull_center);
    Vec para = rotate_vector(Vec(a - b), k_pi*0.5);
    if (angle_between( from, para ) >= angle_between( from, -para )) {
        para *= -1.;
    }
    return para;
}

template <typename OnHolesFunc, typename OnFillsFunc>
void for_each_holes_and_fills
    (BezierTripleIter beg, BezierTripleIter end,
     OnHolesFunc && on_holes, OnFillsFunc && on_fills)
{
    static auto is_hole = [](const BezierTriple & triple)
        { return std::get<1>(triple) == k_no_location; };
    static auto is_not_hole = [](const BezierTriple & triple)
        { return !is_hole(triple); };
    auto handle_seq = [&on_fills, &on_holes]
        (bool seq_is_hole, BezierTripleIter beg, BezierTripleIter end, int idx)
    {
        // having an integer index seems like the least intrusive way to do it
        if (seq_is_hole) { assert(std::all_of(beg, end, is_hole    )); }
        else             { assert(std::all_of(beg, end, is_not_hole)); }
        if (seq_is_hole) { on_holes(beg, end, idx); }
        else             { on_fills(beg, end, idx); }
    };

    auto itr = beg;
    bool seq_is_hole = is_hole(*itr);
    int count_in_first = 0;

    for (auto jtr = itr; jtr != end; ++jtr) {
        if (is_hole(*jtr) != seq_is_hole) {
            if (count_in_first) {
                handle_seq(seq_is_hole, itr, jtr, itr - beg);
            }
            if (!count_in_first) count_in_first = (jtr - itr);
            seq_is_hole = is_hole(*jtr);
            itr = jtr;
        }
    }
    if (count_in_first == 0) {
        handle_seq(seq_is_hole, beg, end, 0);
    } else if (seq_is_hole == is_hole(*beg)) {
        auto needed = std::size_t((end - itr) + count_in_first);
        auto first_end = beg + count_in_first;
        std::vector<BezierTriple> temp;
        temp.reserve(needed);
        std::copy(beg, first_end, std::copy(itr, end, std::back_insert_iterator(temp)));
        handle_seq(seq_is_hole, temp.begin(), temp.end(), itr - beg);
    } else {
        handle_seq(seq_is_hole, itr, end, itr - beg);
        handle_seq(is_hole(*beg), beg, beg + count_in_first, 0);
    }
}

// ----------------------------------------------------------------------------

template <typename IterType, typename T>
bool contains_point
    (IterType beg, IterType end,
     const cul::Vector2<T> & center, const cul::Vector2<T> & pt)
{
    using FVec = cul::Vector2<T>;
    bool rv = false;
    for_side_by_side_wrap(beg, end, [&] (FVec a, FVec b) {
        using namespace cul::fc_signal;
        auto gv = find_intersection(VectorD(a), VectorD(b), VectorD(center), VectorD(pt));
        if (gv == k_no_intersection) {
            return k_continue;
        }
        rv = true;
        return k_break;
    });
    return rv;
}

} // end of <anonymous> namespace
