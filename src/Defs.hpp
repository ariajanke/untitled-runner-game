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

#include <stdexcept>
#include <random>
#include <array>
#include <algorithm>
#include <iosfwd>
#include <memory>
#include <map>

#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Color.hpp>

#include <common/Util.hpp>
#include <common/StringUtil.hpp>
#include <common/Grid.hpp>

#include <ecs/ecs.hpp>

using VectorD = sf::Vector2<double>;
using VectorI = sf::Vector2<int>;
using Rect    = sf::Rect<double>;

constexpr const double k_error = 0.00005;
constexpr const double k_pi    = get_pi<double>();
constexpr const double k_inf   = std::numeric_limits<double>::infinity();

extern const VectorD k_no_intersection;
extern const VectorD k_gravity        ;

struct StartupOptions {
    std::string test_map = "test-map.tmx";
    bool quit_before_game = false;
};

template <typename IterType>
class View {
public:
    View(IterType beg_, IterType end_): m_beg(beg_), m_end(end_) {}
    IterType begin() const { return m_beg; }
    IterType end  () const { return m_end; }
private:
    IterType m_beg, m_end;
};

template <typename T>
View<typename std::vector<T>::const_reverse_iterator>
    make_reverse_view(const std::vector<T> & cont)
{ return View(cont.rbegin(), cont.rend()); }

struct LineSegment {
    enum End { k_a, k_b, k_neither };
    static constexpr const double k_a_side_pos = 0.;
    static constexpr const double k_b_side_pos = 1.;
    LineSegment() {}
    LineSegment(VectorD a_, VectorD b_):
        a(a_), b(b_)
    {}
    LineSegment(double ax, double ay, double bx, double by):
        a(ax, ay), b(bx, by)
    {}
    VectorD a, b;
};

using LineSegmentEnd = decltype(LineSegment::k_a);

struct SurfaceDetails {
    double friction    = 0.145;
    double stop_speed  = 20;
    bool hard_ceilling = false;
};

bool are_same(const SurfaceDetails & rhs, const SurfaceDetails & lhs);

inline bool operator == (const SurfaceDetails & rhs, const SurfaceDetails & lhs)
    { return are_same(rhs, lhs); }

inline bool operator != (const SurfaceDetails & rhs, const SurfaceDetails & lhs)
    { return !are_same(rhs, lhs); }

struct Surface final : public LineSegment, public SurfaceDetails {
    Surface() {}
    explicit Surface(const LineSegment & rhs): LineSegment(rhs) {}
    Surface(const LineSegment & seg_, const SurfaceDetails & dets_):
        LineSegment(seg_), SurfaceDetails(dets_)
    {}
};

class LineMap;
class LineMapLayer;
enum class Layer : uint8_t { foreground, background, neither };
const char * to_string(Layer);

/// like it's member function varient EXCEPT it handles infinities
/// @param r both components must be real numbers
template <typename T>
bool rect_contains(const sf::Rect<T> &, const sf::Vector2<T> & r);

// ------------------------- LineSegments / Surfaces --------------------------

LineSegment move_segment(const LineSegment &, VectorD);

Surface move_surface(const Surface &, VectorD);

inline double segment_length(const LineSegment & seg)
    { return magnitude(seg.a - seg.b); }

inline VectorD location_along(double x, const LineSegment & seg)
    { return (seg.b - seg.a)*x + seg.a; }

inline VectorD velocity_along(double spd, const LineSegment & seg)
    { return (seg.b - seg.a)*spd; }

VectorD find_intersection(const Rect &, VectorD r, VectorD u);

bool line_crosses_rectangle(const Rect &, VectorD, VectorD);

Layer switch_layer(Layer l);

// ----------------------------------- misc -----------------------------------

VectorD find_intersection(const LineSegment &, VectorD old, VectorD new_);

VectorD find_intersection(VectorD a_first, VectorD a_second, VectorD b_first, VectorD b_second);

template <typename T, typename U>
std::enable_if_t<std::is_integral_v<T> && std::is_floating_point_v<U>, T>
    round_to(U u)
{ return T(std::round(u)); }

template <typename T, typename U>
std::enable_if_t<std::is_integral_v<T> && std::is_floating_point_v<U>, sf::Vector2<T>>
    round_to(const sf::Vector2<U> & r)
{ return sf::Vector2<T>(round_to<T>(r.x), round_to<T>(r.y)); }

template <typename T>
inline bool are_very_close(const sf::Vector2<T> & r, const sf::Vector2<T> & u) {
    auto diff = r - u;
    return (diff.x*diff.x + diff.y*diff.y) < k_error*k_error;
}

template <typename T>
std::enable_if_t<std::is_arithmetic_v<T>, bool> are_very_close(T a, T b) {
    return magnitude(a - b) < k_error;
}
#if 0
template <typename T>
inline T & get_null_reference() {
    T * rv = nullptr;
    return *rv;
}
#endif
template <typename RngType>
sf::Color random_color(RngType & rng);

template <typename Rng, typename T>
const T & choose_random(Rng &, std::initializer_list<T>);

uint8_t component_average(int total_steps, int step, uint8_t begin, uint8_t end);

template <typename T>
inline std::ostream & operator << (std::ostream & out, const sf::Vector2<T> & r)
    { return (out << r.x << ", " << r.y); }

template <typename T>
inline std::ostream & operator << (std::ostream & out, const sf::Rect<T> & rect) {
    return (out
            << rect.left << ", " << rect.top << ", w: " << rect.width
            << ", h: " << rect.height);
}

template <typename T>
std::size_t ref_to_index(const std::vector<T> & cont, const T & ref) {
    if (&ref < &cont[0] || &ref >= (&cont[0] + cont.size())) {
        throw std::invalid_argument("ref does not belong to the container");
    }
    return std::size_t(&ref - &cont[0]);
}

template <typename Type1, typename Type2>
bool are_same_size(const Grid<Type1> & lhs, const Grid<Type2> & rhs)
    { return lhs.width() == rhs.width() && lhs.height() == rhs.height(); }

inline bool is_whitespace(char c)
    { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

inline bool is_comma    (char c) { return c == ','; }
inline bool is_colon    (char c) { return c == ':'; }
inline bool is_semicolon(char c) { return c == ';'; }

class BadBranchException final : public std::exception {
public:
    const char * what() const noexcept override {
        return "\"impossible\" branch, this exception is thrown when a branch "
               "is supposed to be \"impossible\" is reached. The fact this "
               "exception was thrown shows a design error.";
    }
};

template <typename ... Types>
using Tuple = std::tuple<Types...>;

template <typename T>
std::tuple<T &, T &> as_tuple(sf::Vector2<T> & r) { return std::tie(r.x, r.y); }

template <typename T>
std::tuple<const T &, const T &> as_tuple(const sf::Vector2<T> & r)
    { return std::tie(r.x, r.y); }

template <typename T>
std::enable_if_t<std::is_arithmetic_v<T>, sf::Rect<T>> expand
    (sf::Rect<T> rv, T amount)
{
    rv.left   -= amount;
    rv.top    -= amount;
    rv.width  += amount*2;
    rv.height += amount*2;
    return rv;
}

template <typename T, typename KeyType, typename SpecTag = TypeTag<T>>
class CachedLoader {
public:
    using LoaderFunction = void (*)(const std::string &, T &);

    template <typename KeyType2>
    std::shared_ptr<T> load(SpecTag, const KeyType2 &);

    void set_loader_function(SpecTag, LoaderFunction);

private:
    static void default_loader(const std::string &, T &) {
        throw std::runtime_error("default_loader: loading resource without setting loader function.");
    }

    std::map<KeyType, std::weak_ptr<T>> m_map;

    LoaderFunction m_loader_func = default_loader;
};

// --------------------------- function definitions ---------------------------

template <typename T>
bool rect_contains(const sf::Rect<T> & rect, const sf::Vector2<T> & r) {
    if (rect.width < 0. || rect.height < 0.) {
        throw std::invalid_argument("rect_contains: negative sized rectangle unhandled.");
    }

    static auto is_within = [](T low, T size, T x) {
        if (!is_real(x)) {
            throw std::invalid_argument("rect_contains: r must be a vector of real number components.");
        }
        if (low == size) return false;
        if constexpr (std::numeric_limits<T>::has_infinity) {
            static constexpr const T k_inf_ = std::numeric_limits<T>::infinity();
            if (low == -k_inf_ && size == k_inf_) {
                return true;
            } else if (low == k_inf_ || low == -k_inf_) {
                return false;
            } else if (size == k_inf_) {
                return x > low;
            }
            if (!is_real(size) || !is_real(low)) {
                throw std::runtime_error("rect_contains: bad branch for non-real numbers");
            }
        }
        return x > low && x < low + size;
    };
    return    is_within(rect.left, rect.width , r.x)
           && is_within(rect.top , rect.height, r.y);
}

template <typename RngType>
sf::Color random_color(RngType & rng) {
    using Uint8Range = std::uniform_int_distribution<uint8_t>;
    sf::Color rv;
    std::array<uint8_t *, 3> ptrs = { &rv.r, &rv.b, &rv.g };
    std::shuffle(ptrs.begin(), ptrs.end(), rng);
    *ptrs[0] = Uint8Range(100, 255)(rng);
    *ptrs[1] = Uint8Range(100, 200)(rng);
    *ptrs[2] = Uint8Range(  0, 100)(rng);
    return rv;
}

template <typename Rng, typename T>
const T & choose_random(Rng & rng, std::initializer_list<T> list) {
    return *(list.begin() + std::uniform_int_distribution<int>(0, list.size() - 1)(rng));
}

template <typename T>
sf::Vector2<T> find_closest_point_to_line
    (const sf::Vector2<T> & a, const sf::Vector2<T> & b,
     const sf::Vector2<T> & external_point)
{
    using Vec = sf::Vector2<T>;
    const auto & c = external_point;
    if (a == b) return a;
    if (a - c == Vec()) return a;
    if (b - c == Vec()) return b;
    // obtuse angles -> snap to extreme points
    auto angle_at_a = angle_between(a - b, a - c);
    if (angle_at_a > get_pi<T>()*0.5) return a;

    auto angle_at_b = angle_between(b - a, b - c);
    if (angle_at_b > get_pi<T>()*0.5) return b;

    // https://www.eecs.umich.edu/courses/eecs380/HANDOUTS/PROJ2/LinePoint.html
    T mag = [&a, &b, &c]() {
        auto num = (c.x - a.x)*(b.x - a.x) + (c.y - a.y)*(b.y - a.y);
        auto denom = magnitude(b - a);
        return num / (denom*denom);
    }();
    return Vec(a.x, a.y) + mag*Vec(b.x - a.x, b.y - a.y);
}

template <typename T>
std::enable_if_t<std::is_floating_point_v<T>, std::tuple<sf::Vector2<T>,sf::Vector2<T>>>
    compute_velocities_to_target
    (const sf::Vector2<T> & source, const sf::Vector2<T> & target,
     const sf::Vector2<T> & influencing_acceleration, T speed)
{
    // Made possible by this wonderful resource:
    // https://www.forrestthewoods.com/blog/solving_ballistic_trajectories/
    //
    // note: this code doesn't quite work yet!
    // I'm adopting 3D code for a 2D game
    using Vec = sf::Vector2<T>;
    using std::make_tuple;
    if (   !is_real(source.x) || !is_real(source.y) || !is_real(speed)
        || !is_real(target.x) || !is_real(target.y) || !is_real(influencing_acceleration.x)
        || !is_real(influencing_acceleration.y))
    {
        throw std::invalid_argument("All arguments must be real numbers");
    }
    if (are_very_close(source, target)) {
        if (are_very_close(influencing_acceleration, Vec())) {
            return make_tuple(Vec(), Vec());
        }
        // return straight up trajectory
        auto s = -normalize(influencing_acceleration)*speed;
        return make_tuple(s, s);
    }
    if (are_very_close(influencing_acceleration, Vec())) {
        // return straight to target
        auto s = normalize(target - source)*speed;
        return make_tuple(s, s);
    }

    // assumptions at this point source != target && acc != 0

    static auto comp_from_basis = [](Vec basis, Vec a) {
        static auto are_parallel = [](Vec a, Vec b)
            { return are_very_close(normalize(a), normalize(b)); };
        auto unita = project_onto(a, basis);
        return magnitude(unita)*T(are_parallel(unita, basis) ? 1 : -1);
    };

    auto j = -normalize(influencing_acceleration);
    auto i = rotate_vector(j, k_pi*0.5);

    T t0, t1;
    {
        auto diff_i = comp_from_basis(i, target - source);
        auto diff_j = comp_from_basis(j, target - source);

        auto spd_sq = speed*speed;
        auto g = magnitude(influencing_acceleration);
        auto do_atan_with_sqpart = [spd_sq, g, diff_i] (T sqpart)
            { return std::atan( (spd_sq + sqpart) / (g*diff_i) ); };

        auto sqpart = std::sqrt(spd_sq*spd_sq - g*(g*diff_i*diff_i + T(2)*spd_sq*diff_j));
        t0 = do_atan_with_sqpart( sqpart);
        t1 = do_atan_with_sqpart(-sqpart);

    }
    auto ground_dir = normalize(project_onto(target - source, i));
    auto up         = -normalize(influencing_acceleration);

    auto s0 = ground_dir*std::cos(t0)*speed + up*std::sin(t0)*speed;
    if (are_very_close(t0, t1)) { return make_tuple(s0, s0); }
    return make_tuple(s0, ground_dir*std::cos(t1)*speed + up*std::sin(t1)*speed);
}

inline bool are_same(const SurfaceDetails & rhs, const SurfaceDetails & lhs) {
    return are_very_close(rhs.friction  , lhs.friction  ) &&
           are_very_close(rhs.stop_speed, lhs.stop_speed) &&
           rhs.hard_ceilling == lhs.hard_ceilling;
}

// ----------------------------------------------------------------------------

template <typename T, typename KeyType, typename SpecTag>
template <typename KeyType2>
std::shared_ptr<T> CachedLoader<T, KeyType, SpecTag>::load(SpecTag, const KeyType2 & key) {
    std::shared_ptr<T> rv;
    auto itr = m_map.find(key);
    bool need_reload = itr == m_map.end() ? true : itr->second.expired();
    if (need_reload) {
        m_loader_func(key, *(rv = std::make_shared<T>()));
        m_map[key] = rv;
    } else {
        rv = itr->second.lock();
    }
    return rv;
}

template <typename T, typename KeyType, typename SpecTag>
void CachedLoader<T, KeyType, SpecTag>::set_loader_function(SpecTag, LoaderFunction fn)
    { m_loader_func = fn; }

// ----------------------------------------------------------------------------

/* Credit to Howard Hinnant. Copyrighted under the CC-BY 4.0 license
 * Source:
 * https://howardhinnant.github.io/allocator_boilerplate.html
 */
template <std::size_t k_in_place_length_t>
struct DefineInPlace {

    template <typename T>
    class Allocator {
    public:
        static constexpr const std::size_t k_in_place_length = k_in_place_length_t;
        using value_type = T;
        Allocator() noexcept {}  // not required, unless used
#       if 0
        template <typename U>
        Allocator(Allocator<U> const &) noexcept = default;
#       endif

        Allocator(const Allocator<T> & rhs): m_storage(rhs.m_storage) {}
        Allocator(Allocator<T> &&) = delete;

        Allocator & operator = (const Allocator<T> & rhs) {
            m_storage = rhs.m_storage;
            return *this;
        }
        Allocator & operator = (Allocator<T> &&) = delete;

        value_type * allocate(std::size_t n) {
            if (n <= k_in_place_length) {
                return reinterpret_cast<value_type *>(&m_storage[0]);
            }
            return static_cast<value_type*>(::operator new (n*sizeof(value_type)));
        }

        void deallocate(value_type * p, std::size_t) noexcept {
            if (p == reinterpret_cast<value_type *>(&m_storage[0])) return;
            ::operator delete(p);
        }

        bool is_same_as(const Allocator<T> & rhs) const noexcept
            { return m_storage.data() == rhs.m_storage.data(); }

    private:
        std::array<
            typename std::aligned_storage<sizeof(T), alignof(T)>::type,
            k_in_place_length> m_storage;
    };
};

template <typename T, std::size_t k_len>
using DefineInPlaceVector = std::vector<T, typename DefineInPlace<k_len>::template Allocator<T>>;

template <typename T>
void reserve_in_place(T & cont) {
    cont.reserve(T::allocator_type::k_in_place_length);
}

template <class T, class U, std::size_t k_in_place_length>
bool operator ==
    (typename DefineInPlace<k_in_place_length>::template Allocator<T> const & lhs,
     typename DefineInPlace<k_in_place_length>::template Allocator<T> const & rhs) noexcept
{ return  lhs.is_same_as(rhs); }

template <class T, class U, std::size_t k_in_place_length>
bool operator !=
    (typename DefineInPlace<k_in_place_length>::template Allocator<T> const & lhs,
     typename DefineInPlace<k_in_place_length>::template Allocator<T> const & rhs) noexcept
{ return !lhs.is_same_as(rhs); }
