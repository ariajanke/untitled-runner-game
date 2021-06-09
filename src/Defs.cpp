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

#include "Defs.hpp"
#include "Components.hpp"
#include "maps/Maps.hpp"
#include <cassert>

namespace {

using Error = std::runtime_error;
#if 0
inline double cross_magnitude(VectorD a, VectorD b)
    { return a.x*b.y - a.y*b.x; }
#endif
} // end of <anonymous> namespace

const char * to_string(Layer layer) {
    switch (layer) {
    case Layer::background: return "background";
    case Layer::foreground: return "foreground";
    case Layer::neither   : return "neither"   ;
    }
    throw BadBranchException();
}

LineSegment move_segment(const LineSegment & segment, VectorD offset) {
    auto rv = segment;
    rv.a += offset;
    rv.b += offset;
    return rv;
}

Surface move_surface(const Surface & surface, VectorD offset) {
    auto rv = surface;
    rv.a += offset;
    rv.b += offset;
    return rv;
}

Layer switch_layer(Layer l) {
    switch (l) {
    case Layer::background: return Layer::foreground;
    case Layer::foreground: return Layer::background;
    case Layer::neither: default:
        throw std::invalid_argument("switch_layer: neither is a sentinel value.");
    }
}

// ----------------------------------------------------------------------------

const VectorD k_no_intersection = cul::get_no_solution_sentinel<VectorD>();
#   if 0
    VectorD(std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity());
#   endif
const VectorD k_gravity = VectorD(0, 667);
#if 0
VectorD find_intersection(VectorD a_first, VectorD a_second, VectorD b_first, VectorD b_second) {
    auto p = a_first;
    auto r = a_second - p;

    auto q = b_first ;
    auto s = b_second - q;

    // http://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
    // two points of early failure
    auto r_cross_s = cross_magnitude(r, s);
    if (r_cross_s == 0.0) return k_no_intersection;

    auto q_sub_p = q - p;
    auto t = cross_magnitude(q_sub_p, s) / r_cross_s;

    if (t < 0. || t > 1.) return k_no_intersection;

    auto u = cross_magnitude(q_sub_p, r) / r_cross_s;
    if (u < 0. || u > 1.) return k_no_intersection;

    return p + t*r;
}
#endif
VectorD find_intersection(const LineSegment & seg, VectorD old, VectorD new_) {
    return find_intersection(seg.a, seg.b, old, new_);
}

template <typename Head, typename ... Types>
Head find_alternative_to(const Head & h, Types...) { return h; }

template <typename Head, typename OtherType, typename ... Types>
Head find_alternative_to(const Head & h, const OtherType & o, Types ... args) {
    if (o != h) return o;
    return find_alternative_to(h, std::forward<Types>(args)...);
}

VectorD find_intersection(const Rect & rect, VectorD r, VectorD u) {
    VectorD tl(rect.left, rect.top);
    VectorD tr = tl + VectorD(rect.width,           0);
    VectorD bl = tl + VectorD(         0, rect.height);
    VectorD br = tl + VectorD(rect.width, rect.height);

    return find_alternative_to(k_no_intersection,
            find_intersection(tl, tr, r, u),
            find_intersection(tl, bl, r, u),
            find_intersection(br, tr, r, u),
            find_intersection(br, bl, r, u));
}

bool line_crosses_rectangle(const Rect & rect, VectorD r, VectorD u) {
    return find_intersection(rect, r, u) != k_no_intersection;
#   if 0
    VectorD tl(rect.left, rect.top);
    VectorD tr = tl + VectorD(rect.width,           0);
    VectorD bl = tl + VectorD(         0, rect.height);
    VectorD br = tl + VectorD(rect.width, rect.height);
    return (find_intersection(tl, tr, r, u) != k_no_intersection ||
            find_intersection(tl, bl, r, u) != k_no_intersection ||
            find_intersection(br, tr, r, u) != k_no_intersection ||
            find_intersection(br, bl, r, u) != k_no_intersection );
#   endif
}

uint8_t component_average(int total_steps, int step, uint8_t b, uint8_t e) {
    assert(total_steps >= step);
    int res = (int(b)*(total_steps - step) + int(e)*step) / total_steps;
    assert(res < 256);
    return uint8_t(res);
}

double truncate_mantissa_to(double x, int bin_digits) {
    static_assert(sizeof(double) == 8, "");
    static_assert(std::numeric_limits<double>::is_iec559, "");
    static constexpr const int k_mantissa_bit_count = 52;
    if (bin_digits < 0 || bin_digits > k_mantissa_bit_count) {
        throw std::invalid_argument("");
    }
    // first 12 are sign and exponent
    static constexpr const uint64_t k_sign_exp_mask =
        //                 1               2               3
        // 0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF
         0b1111111111110000000000000000000000000000000000000000000000000000;
    static constexpr const uint64_t k_mantissa_mask = ~k_sign_exp_mask;
    const uint64_t & x_as_u64 = reinterpret_cast<const uint64_t &>(x);
    uint64_t x_bits_to_push = x_as_u64 & k_mantissa_mask;
    x_bits_to_push >>= (k_mantissa_bit_count - bin_digits);
    x_bits_to_push <<= (k_mantissa_bit_count - bin_digits);
    auto prod = (k_sign_exp_mask & x_as_u64) | x_bits_to_push;
    return reinterpret_cast<const double &>(prod);
}

VectorD truncate_mantissa_to(VectorD r, int bin_digits) {
    return VectorD(truncate_mantissa_to(r.x, bin_digits),
                   truncate_mantissa_to(r.y, bin_digits));
}
