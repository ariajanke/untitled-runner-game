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
extern const VectorD k_no_intersection;
extern const VectorD k_gravity        ;

struct StartupOptions {
    std::string test_map = "test-map.tmx";
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
#if 0
// ---------------------- special floating point helpers ----------------------

// first  low  bounds where f(first ) = false
// second high bounds where f(second) = true
template <typename Func>
std::pair<double, double> find_smallest_diff(Func && f, double hint = 0);

template <typename Func>
double find_highest_false(Func && f, double hint = 0.5) {
    return find_smallest_diff(std::move(f), hint).first;
}

template <typename Func>
double find_lowest_true(Func && f, double hint = 0.5) {
    return find_smallest_diff(std::move(f), hint).second;
}
#endif
// ------------------------- LineSegments / Surfaces --------------------------

LineSegment move_segment(const LineSegment &, VectorD);

Surface move_surface(const Surface &, VectorD);

inline double segment_length(const LineSegment & seg)
    { return magnitude(seg.a - seg.b); }

inline VectorD location_along(double x, const LineSegment & seg)
    { return (seg.b - seg.a)*x + seg.a; }

inline VectorD velocity_along(double spd, const LineSegment & seg)
    { return (seg.b - seg.a)*spd; }

bool line_crosses_rectangle(const Rect &, VectorD, VectorD);

Layer switch_layer(Layer l);

// ----------------------------------- misc -----------------------------------

VectorD find_intersection(const LineSegment &, VectorD old, VectorD new_);

VectorD find_intersection(VectorD a_first, VectorD a_second, VectorD b_first, VectorD b_second);

template <typename T>
inline bool are_very_close(const sf::Vector2<T> & r, const sf::Vector2<T> & u) {
    auto diff = r - u;
    return (diff.x*diff.x + diff.y*diff.y) < k_error*k_error;
}

template <typename T>
std::enable_if_t<std::is_arithmetic_v<T>, bool> are_very_close(T a, T b) {
    return magnitude(a - b) < k_error;
}

template <typename T>
inline T & get_null_reference() {
    T * rv = nullptr;
    return *rv;
}

template <typename RngType>
sf::Color random_color(RngType & rng);

template <typename Rng, typename T>
const T & choose_random(Rng, std::initializer_list<T>);

uint8_t component_average(int total_steps, int step, uint8_t begin, uint8_t end);

template <typename T>
inline std::ostream & operator << (std::ostream & out, const sf::Vector2<T> & r)
    { return (out << r.x << ", " << r.y); }
#if 0
template <IsCharacterFunction is_char, typename Func>
inline void for_split(const std::string & str, Func && f) {
    const auto * beg = &str[0];
    ::for_split<is_char>(beg, beg + str.length(), std::move(f));
}
#endif
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

class ImpossibleBranchException final : public std::exception {
public:
    const char * what() const noexcept override {
        return "\"impossible\" branch, this exception is thrown when a branch "
               "is supposed to be \"impossible\" is reached. The fact this "
               "exception was thrown shows a design error.";
    }
};

template <typename T, typename SpecTag = TypeTag<T>>
class CachedLoader {
public:
    using LoaderFunction = void (*)(const std::string &, T &);

    template <typename KeyType>
    std::shared_ptr<T> load(SpecTag, const KeyType &);

    void set_loader_function(SpecTag, LoaderFunction);

private:
    static void default_loader(const std::string &, T &) {
        throw std::runtime_error("default_loader: loading resource without setting loader function.");
    }

    std::map<std::string, std::weak_ptr<T>> m_map;

    LoaderFunction m_loader_func = default_loader;
};

// --------------------------- function definitions ---------------------------
#if 0
template <typename Func>
std::pair<double, double> find_smallest_diff(Func && f, double hint) {
    if (f(0)) {
        throw std::runtime_error("find_smallest_diff: f(0) is true");
    }
    if (!f(1)) {
        throw std::runtime_error("find_smallest_diff: f(1) is false");
    }

    static constexpr const double k_good_enough = k_error;
    bool   fg   = f(hint);
    double low  = fg ? 0    : hint;
    double high = fg ? hint : 1   ;

    while ((high - low) > k_good_enough) {
        double t = low + (high - low)*0.5;
        if (f(t)) {
            high = t;
        } else {
            low = t;
        }
    }
    return std::make_pair(low, high);
}
#endif
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
const T & choose_random(Rng rng, std::initializer_list<T> list) {
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

inline bool are_same(const SurfaceDetails & rhs, const SurfaceDetails & lhs) {
    return are_very_close(rhs.friction  , lhs.friction  ) &&
           are_very_close(rhs.stop_speed, lhs.stop_speed) &&
           rhs.hard_ceilling == lhs.hard_ceilling;
}

// ----------------------------------------------------------------------------

template <typename T, typename SpecTag>
template <typename KeyType>
std::shared_ptr<T> CachedLoader<T, SpecTag>::load(SpecTag, const KeyType & key) {
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

template <typename T, typename SpecTag>
void CachedLoader<T, SpecTag>::set_loader_function(SpecTag, LoaderFunction fn)
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
#if 0
template <typename T, std::size_t k_in_place_length>
auto make_in_place_vector() {
    std::vector<T, typename DefineInPlace<k_in_place_length>::template Allocator<T>> rv;
    rv.reserve(k_in_place_length);
    return rv;
}
#endif
