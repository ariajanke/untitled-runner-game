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

#include "get_8x8_char.hpp"

#include <cassert>
#include <cstring>

namespace {

const char * check_8x8_char(const char *);

const char * get_8x8_char_impl(UChar c);

} // end of <anonymous> namespace

const char * get_8x8_char(UChar c)
    { return check_8x8_char(get_8x8_char_impl(c)); }

bool is_on_pixel(char c) { return c != ' '; }

namespace {

const char * get_8x8_char_impl(UChar c) {
    switch (c) {
    // ------------------------------- Capitals -------------------------------
    case 'A': return
        // 234567
        "        "  // 0
        "   XXX  "  // 1
        "  X   X "  // 2
        "  XXXXX "  // 3
        "  X   X "  // 4
        "  X   X "  // 5
        "        "  // 6
        "        "; // 7
        // 234567
    case 'B': return
        // 234567
        "        "  // 0
        "  XXXX  "  // 1
        "  X   X "  // 2
        "  XXXX  "  // 3
        "  X   X "  // 4
        "  XXXX  "  // 5
        "        "  // 6
        "        "; // 7
        // 234567
    case 'C': return
        // 234567
        "        "  // 0
        "   XXXX "  // 1
        "  X     "  // 2
        "  X     "  // 3
        "  X     "  // 4
        "   XXXX "  // 5
        "        "  // 6
        "        "; // 7
        // 234567
    case 'D': return
        // 234567
        "        "  // 0
        "  XXXX  "  // 1
        "  X   X "  // 2
        "  X   X "  // 3
        "  X   X "  // 4
        "  XXXX  "  // 5
        "        "  // 6
        "        "; // 7
        // 234567
    case 'E': return
        // 234567
        "        "  // 0
        "  XXXXX "  // 1
        "  X     "  // 2
        "  XXXXX "  // 3
        "  X     "  // 4
        "  XXXXX "  // 5
        "        "  // 6
        "        "; // 7
        // 234567
    case 'F': return
        // 234567
        "        "  // 0
        "  XXXXX "  // 1
        "  X     "  // 2
        "  XXXXX "  // 3
        "  X     "  // 4
        "  X     "  // 5
        "        "  // 6
        "        "; // 7
        // 234567
    case 'G': return
        // 234567
        "        "  // 0
        "   XXX  "  // 1
        "  X     "  // 2
        "  X  XX "  // 3
        "  X   X "  // 4
        "   XXX  "  // 5
        "        "  // 6
        "        "; // 7
        // 234567
    case 'H': return
        // 234567
        "        "  // 0
        "  X   X "  // 1
        "  X   X "  // 2
        "  XXXXX "  // 3
        "  X   X "  // 4
        "  X   X "  // 5
        "        "  // 6
        "        "; // 7
        // 234567
    case 'I': return
        // 234567
        "        "  // 0
        "  XXXXX "  // 1
        "    X   "  // 2
        "    X   "  // 3
        "    X   "  // 4
        "  XXXXX "  // 5
        "        "  // 6
        "        "; // 7
        // 234567
    case 'J': return
        // 234567
        "        "  // 0
        "  XXXXX "  // 1
        "     X  "  // 2
        "     X  "  // 3
        "     X  "  // 4
        "  XXXX  "  // 5
        "        "  // 6
        "        "; // 7
        // 234567
    case 'K': return
        // 234567
        "        "  // 0
        "  X  X  "  // 1
        "  X X   "  // 2
        "  XX    "  // 3
        "  X X   "  // 4
        "  X  X  "  // 5
        "        "  // 6
        "        "; // 7
        // 234567
    case 'L': return
        // 234567
        "        "  // 0
        "  X     "  // 1
        "  X     "  // 2
        "  X     "  // 3
        "  X     "  // 4
        "  XXXXX "  // 5
        "        "  // 6
        "        "; // 7
        // 234567
    case 'M': return
        // 234567
        "        "  // 0
        "  X   X "  // 1
        "  XX XX "  // 2
        "  X X X "  // 3
        "  X   X "  // 4
        "  X   X "  // 5
        "        "  // 6
        "        "; // 7
        // 234567
    case 'N': return
        // 234567
        "        "  // 0
        "  X   X "  // 1
        "  XX  X "  // 2
        "  X X X "  // 3
        "  X  XX "  // 4
        "  X   X "  // 5
        "        "  // 6
        "        "; // 7
        // 234567
    case 'O': return
        // 234567
        "        "  // 0
        "   XXX  "  // 1
        "  X   X "  // 2
        "  X   X "  // 3
        "  X   X "  // 4
        "   XXX  "  // 5
        "        "  // 6
        "        "; // 7
        // 234567
    case 'P': return
        // 234567
        "        "  // 0
        "  XXXX  "  // 1
        "  X   X "  // 2
        "  XXXX  "  // 3
        "  X     "  // 4
        "  X     "  // 5
        "        "  // 6
        "        "; // 7
        // 234567
    case 'Q': return
        // 234567
        "        "  // 0
        "   XXX  "  // 1
        "  X   X "  // 2
        "  X X X "  // 3
        "  X  X  "  // 4
        "   XX X "  // 5
        "        "  // 6
        "        "; // 7
        // 234567
    case 'R': return
        // 234567
        "        "  // 0
        "  XXXX  "  // 1
        "  X   X "  // 2
        "  XXXX  "  // 3
        "  X  X  "  // 4
        "  X   X "  // 5
        "        "  // 6
        "        "; // 7
        // 234567
    case 'S': return
        // 234567
        "        "  // 0
        "   XXXX "  // 1
        "  X     "  // 2
        "   XXX  "  // 3
        "      X "  // 4
        "  XXXX  "  // 5
        "        "  // 6
        "        "; // 7
        // 234567
    case 'T': return
        // 234567
        "        "  // 0
        "  XXXXX "  // 1
        "    X   "  // 2
        "    X   "  // 3
        "    X   "  // 4
        "    X   "  // 5
        "        "  // 6
        "        "; // 7
        // 234567
    case 'U': return
        // 234567
        "        "  // 0
        "  X   X "  // 1
        "  X   X "  // 2
        "  X   X "  // 3
        "  X   X "  // 4
        "   XXX  "  // 5
        "        "  // 6
        "        "; // 7
        // 234567
    case 'V': return
        // 234567
        "        "  // 0
        "  X   X "  // 1
        "  X   X "  // 2
        "   X X  "  // 3
        "   X X  "  // 4
        "    X   "  // 5
        "        "  // 6
        "        "; // 7
        // 234567
    case 'W': return
        // 234567
        "        "  // 0
        "  X   X "  // 1
        "  X   X "  // 2
        "  X X X "  // 3
        "  XX XX "  // 4
        "  X   X "  // 5
        "        "  // 6
        "        "; // 7
    case 'X': return
        // 234567
        "        "  // 0
        "  X   X "  // 1
        "   X X  "  // 2
        "    X   "  // 3
        "   X X  "  // 4
        "  X   X "  // 5
        "        "  // 6
        "        "; // 7
    case 'Y': return
        // 234567
        "        "  // 0
        "  X   X "  // 1
        "   X X  "  // 2
        "    X   "  // 3
        "    X   "  // 4
        "    X   "  // 5
        "        "  // 6
        "        "; // 7
    case 'Z': return
        // 234567
        "        "  // 0
        "  XXXXX "  // 1
        "     X  "  // 2
        "    X   "  // 3
        "   X    "  // 4
        "  XXXXX "  // 5
        "        "  // 6
        "        "; // 7
    // ------------------------------ Lowercase -------------------------------
    case 'a': return
        // 234567
        "        "  // 0
        "        "  // 1
        "   XX   "  // 2
        "  X  X  "  // 3
        "  X  X  "  // 4
        "   XX X "  // 5
        "        "  // 6
        "        "; // 7
    case 'b': return
        // 234567
        "        "  // 0
        "  X     "  // 1
        "  X     "  // 2
        "  XXXX  "  // 3
        "  X   X "  // 4
        "  XXXX  "  // 5
        "        "  // 6
        "        "; // 7
    case 'c': return
        // 234567
        "        "  // 0
        "        "  // 1
        "   XXXX "  // 2
        "  X     "  // 3
        "  X     "  // 4
        "   XXXX "  // 5
        "        "  // 6
        "        "; // 7
    case 'd': return
        // 234567
        "        "  // 0
        "      X "  // 1
        "      X "  // 2
        "   XXXX "  // 3
        "  X   X "  // 4
        "   XXXX "  // 5
        "        "  // 6
        "        "; // 7
    case 'e': return
        // 234567
        "        "  // 0
        "   XXX  "  // 1
        "  X   X "  // 2
        "  XXXXX "  // 3
        "  X     "  // 4
        "   XXX  "  // 5
        "        "  // 6
        "        "; // 7
    case 'f': return
        // 234567
        "        "  // 0
        "    XX  "  // 1
        "   X    "  // 2
        "  XXXX  "  // 3
        "   X    "  // 4
        "   X    "  // 5
        "        "  // 6
        "        "; // 7
    case 'g': return
        // 234567
        "        "  // 0
        "        "  // 1
        "   XXX  "  // 2
        "  X   X "  // 3
        "   XXXX "  // 4
        "      X "  // 5
        "   XXX  "  // 6
        "        "; // 7
    case 'h': return
        // 234567
        "        "  // 0
        "  X     "  // 1
        "  X     "  // 2
        "  XXXX  "  // 3
        "  X   X "  // 4
        "  X   X "  // 5
        "        "  // 6
        "        "; // 7
    case 'i': return
        // 234567
        "        "  // 0
        "    X   "  // 1
        "        "  // 2
        "   XX   "  // 3
        "    X   "  // 4
        "    X   "  // 5
        "        "  // 6
        "        "; // 7
    case 'j': return
        // 234567
        "        "  // 0
        "     X  "  // 1
        "        "  // 2
        "    XX  "  // 3
        "     X  "  // 4
        "     X  "  // 5
        "   XX   "  // 6
        "        "; // 7
    case 'k': return
        // 234567
        "        "  // 0
        "  X     "  // 1
        "  X     "  // 2
        "  XXXX  "  // 3
        "  X X   "  // 4
        "  X  X  "  // 5
        "        "  // 6
        "        "; // 7
    case 'l': return
        // 234567
        "        "  // 0
        "   XX   "  // 1
        "    X   "  // 2
        "    X   "  // 3
        "    X   "  // 4
        "    XX  "  // 5
        "        "  // 6
        "        "; // 7
    case 'm': return
        // 234567
        "        "  // 0
        "        "  // 1
        "   X X  "  // 2
        "  X X X "  // 3
        "  X X X "  // 4
        "  X   X "  // 5
        "        "  // 6
        "        "; // 7
    case 'n': return
        // 234567
        "        "  // 0
        "        "  // 1
        "  XXX   "  // 2
        "  X  X  "  // 3
        "  X   X "  // 4
        "  X   X "  // 5
        "        "  // 6
        "        "; // 7
    case 'o': return
        // 234567
        "        "  // 0
        "        "  // 1
        "   XXX  "  // 2
        "  X   X "  // 3
        "  X   X "  // 4
        "   XXX  "  // 5
        "        "  // 6
        "        "; // 7
    case 'p': return
        // 234567
        "        "  // 0
        "        "  // 1
        "   XXX  "  // 2
        "  X   X "  // 3
        "  XXXX  "  // 4
        "  X     "  // 5
        "  X     "  // 6
        "        "; // 7
    case 'q': return
        // 234567
        "        "  // 0
        "        "  // 1
        "   XXX  "  // 2
        "  X   X "  // 3
        "   XXXX "  // 4
        "      X "  // 5
        "      X "  // 6
        "        "; // 7
    case 'r': return
        // 234567
        "        "  // 0
        "        "  // 1
        "  X XXX "  // 2
        "  XX    "  // 3
        "  X     "  // 4
        "  X     "  // 5
        "        "  // 6
        "        "; // 7
    case 's': return
        // 234567
        "        "  // 0
        "        "  // 1
        "   XXX  "  // 2
        "  X     "  // 3
        "   XXX  "  // 4
        "      X "  // 5
        "   XXX  "  // 6
        "        "; // 7
    case 't': return
        // 234567
        "        "  // 0
        "        "  // 1
        "    X   "  // 2
        "   XXX  "  // 3
        "    X   "  // 4
        "    XX  "  // 5
        "        "  // 6
        "        "; // 7
    case 'u': return
        // 234567
        "        "  // 0
        "        "  // 1
        "  X   X "  // 2
        "  X   X "  // 3
        "  X   X "  // 4
        "   XXXX "  // 5
        "        "  // 6
        "        "; // 7
    case 'v': return
        // 234567
        "        "  // 0
        "        "  // 1
        "  X   X "  // 2
        "  X   X "  // 3
        "   X X  "  // 4
        "    X   "  // 5
        "        "  // 6
        "        "; // 7
    case 'w': return
        // 234567
        "        "  // 0
        "        "  // 1
        "  X   X "  // 2
        "  X X X "  // 3
        "  X X X "  // 4
        "   X X  "  // 5
        "        "  // 6
        "        "; // 7
    case 'x': return
        // 234567
        "        "  // 0
        "        "  // 1
        "   X  X "  // 2
        "    XX  "  // 3
        "    XX  "  // 4
        "   X  X "  // 5
        "        "  // 6
        "        "; // 7
    case 'y': return
        // 234567
        "        "  // 0
        "        "  // 1
        "        "  // 2
        "  X   X "  // 3
        "   XXX  "  // 4
        "     X  "  // 5
        "   XX   "  // 6
        "        "; // 7
    case 'z': return
        // 234567
        "        "  // 0
        "        "  // 1
        "  XXXXX "  // 2
        "    XX  "  // 3
        "   XX   "  // 4
        "  XXXXX "  // 5
        "        "  // 6
        "        "; // 7
    // ------------------------ Punctuation / Symbols -------------------------
    case '`': return
        // 234567
        "        "  // 0
        "  XX    "  // 1
        "   X    "  // 2
        "        "  // 3
        "        "  // 4
        "        "  // 5
        "        "  // 6
        "        "; // 7
    case '-': return
        // 234567
        "        "  // 0
        "        "  // 1
        "        "  // 2
        "  XXXXX "  // 3
        "        "  // 4
        "        "  // 5
        "        "  // 6
        "        "; // 7
    case '=': return
        // 234567
        "        "  // 0
        "        "  // 1
        "  XXXXX "  // 2
        "        "  // 3
        "  XXXXX "  // 4
        "        "  // 5
        "        "  // 6
        "        "; // 7
    case '[': return
        // 234567
        "        "  // 0
        "   XX   "  // 1
        "   X    "  // 2
        "   X    "  // 3
        "   X    "  // 4
        "   XX   "  // 5
        "        "  // 6
        "        "; // 7
    case ']': return
        // 234567
        "        "  // 0
        "    XX  "  // 1
        "     X  "  // 2
        "     X  "  // 3
        "     X  "  // 4
        "    XX  "  // 5
        "        "  // 6
        "        "; // 7
    case '\\': return
        // 234567
        "        "  // 0
        "  X     "  // 1
        "   X    "  // 2
        "    X   "  // 3
        "     X  "  // 4
        "      X "  // 5
        "        "  // 6
        "        "; // 7
    case ';': return
        // 234567
        "        "  // 0
        "        "  // 1
        "     X  "  // 2
        "        "  // 3
        "     X  "  // 4
        "    X   "  // 5
        "        "  // 6
        "        "; // 7
    case '\'': return
        // 234567
        "        "  // 0
        "     X  "  // 1
        "     X  "  // 2
        "        "  // 3
        "        "  // 4
        "        "  // 5
        "        "  // 6
        "        "; // 7
    case ',': return
        // 234567
        "        "  // 0
        "        "  // 1
        "        "  // 2
        "        "  // 3
        "        "  // 4
        "     X  "  // 5
        "    X   "  // 6
        "        "; // 7
    case '.': return
        // 234567
        "        "  // 0
        "        "  // 1
        "        "  // 2
        "        "  // 3
        "        "  // 4
        "     X  "  // 5
        "        "  // 6
        "        "; // 7
    case '/': return
        // 234567
        "        "  // 0
        "      X "  // 1
        "     X  "  // 2
        "    X   "  // 3
        "   X    "  // 4
        "  X     "  // 5
        "        "  // 6
        "        "; // 7
    case '~':  return
        // 234567
        "        "  // 0
        "        "  // 1
        "   X    "  // 2
        "  X X X "  // 3
        "     X  "  // 4
        "        "  // 5
        "        "  // 6
        "        "; // 7
    case '_': return
       // 234567
       "        "  // 0
       "        "  // 1
       "        "  // 2
       "        "  // 3
       "        "  // 4
       "  XXXXX "  // 5
       "        "  // 6
       "        "; // 7
    case '+': return
        // 234567
        "        "  // 0
        "    X   "  // 1
        "    X   "  // 2
        "  XXXXX "  // 3
        "    X   "  // 4
        "    X   "  // 5
        "        "  // 6
        "        "; // 7
    case '{': return
        // 234567
        "        "  // 0
        "    XX  "  // 1
        "   X    "  // 2
        "  XX    "  // 3
        "   X    "  // 4
        "    XX  "  // 5
        "        "  // 6
        "        "; // 7
    case '}': return
        // 234567
        "        "  // 0
        "   XX   "  // 1
        "     X  "  // 2
        "     XX "  // 3
        "     X  "  // 4
        "   XX   "  // 5
        "        "  // 6
        "        "; // 7
    case '|': return
        // 234567
        "        "  // 0
        "    X   "  // 1
        "    X   "  // 2
        "    X   "  // 3
        "    X   "  // 4
        "    X   "  // 5
        "    X   "  // 6
        "        "; // 7
    case ':': return
        // 234567
        "        "  // 0
        "    XX  "  // 1
        "    XX  "  // 2
        "        "  // 3
        "    XX  "  // 4
        "    XX  "  // 5
        "        "  // 6
        "        "; // 7
    case '"': return
        // 234567
        "        "  // 0
        "   X X  "  // 1
        "   X X  "  // 2
        "        "  // 3
        "        "  // 4
        "        "  // 5
        "        "  // 6
        "        "; // 7
    case '<': return
        // 234567
        "        "  // 0
        "    X   "  // 1
        "   X    "  // 2
        "  X     "  // 3
        "   X    "  // 4
        "    X   "  // 5
        "        "  // 6
        "        "; // 7
    case '>': return
        // 234567
        "        "  // 0
        "    X   "  // 1
        "     X  "  // 2
        "      X "  // 3
        "     X  "  // 4
        "    X   "  // 5
        "        "  // 6
        "        "; // 7
    case '?': return
        // 234567
        "        "  // 0
        "    XX  "  // 1
        "   X  X "  // 2
        "      X "  // 3
        "    XX  "  // 4
        "        "  // 5
        "    X   "  // 6
        "        "; // 7
    // ------------------------------- Numerics -------------------------------
    case '0': return
        // 234567
        "        "  // 0
        "   XXX  "  // 1
        "  X   X "  // 2
        "  X X X "  // 3
        "  X   X "  // 4
        "   XXX  "  // 5
        "        "  // 6
        "        "; // 7
    case '1': return
        // 234567
        "        "  // 0
        "   XX   "  // 1
        "    X   "  // 2
        "    X   "  // 3
        "    X   "  // 4
        "  xXXXX "  // 5
        "        "  // 6
        "        "; // 7
    case '2': return
        // 234567
        "        "  // 0
        "   XXX  "  // 1
        "      X "  // 2
        "   XXX  "  // 3
        "  X     "  // 4
        "  XXXXX "  // 5
        "        "  // 6
        "        "; // 7
    case '3': return
        // 234567
        "        "  // 0
        "  XXXX  "  // 1
        "      X "  // 2
        "   XXX  "  // 3
        "      X "  // 4
        "  XXXX  "  // 5
        "        "  // 6
        "        "; // 7
    case '4': return
        // 234567
        "        "  // 0
        "  X  X  "  // 1
        "  X  X  "  // 2
        "  XXXXX "  // 3
        "     X  "  // 4
        "     X  "  // 5
        "        "  // 6
        "        "; // 7
    case '5': return
        // 234567
        "        "  // 0
        "  XXXXX "  // 1
        "  X     "  // 2
        "   XXX  "  // 3
        "      X "  // 4
        "   XXX  "  // 5
        "        "  // 6
        "        "; // 7
    case '6':  return
        // 234567
        "        "  // 0
        "   XXX  "  // 1
        "  X     "  // 2
        "  XXXX  "  // 3
        "  X   X "  // 4
        "   XXX  "  // 5
        "        "  // 6
        "        "; // 7
    case '7': return
        // 234567
        "        "  // 0
        "  XXXXX "  // 1
        "      X "  // 2
        "    X   "  // 3
        "    X   "  // 4
        "    X   "  // 5
        "        "  // 6
        "        "; // 7
    case '8': return
        // 234567
        "        "  // 0
        "   XXX  "  // 1
        "  X   X "  // 2
        "   XXX  "  // 3
        "  X   X "  // 4
        "   XXX  "  // 5
        "        "  // 6
        "        "; // 7
    case '9': return
        // 234567
        "        "  // 0
        "   XXX  "  // 1
        "  X   X "  // 2
        "   XXXX "  // 3
        "      X "  // 4
        "   XXX  "  // 5
        "        "  // 6
        "        "; // 7
    // ------------------------- Symbols on Numerics --------------------------
    case ')': return
        // 234567
        "        "  // 0
        "    X   "  // 1
        "     X  "  // 2
        "     X  "  // 3
        "     X  "  // 4
        "    X   "  // 5
        "        "  // 6
        "        "; // 7
    case '!': return
        // 234567
        "        "  // 0
        "    X   "  // 1
        "    X   "  // 2
        "    X   "  // 3
        "    X   "  // 4
        "        "  // 5
        "    X   "  // 6
        "        "; // 7
    case '@': return
        // 234567
        "        "  // 0
        "   XXX  "  // 1
        "  X   X "  // 2
        "  X XXX "  // 3
        "  X     "  // 4
        "   XXX  "  // 5
        "        "  // 6
        "        "; // 7
    case '#': return
        // 234567
        "        "  // 0
        "   X X  "  // 1
        "  XXXXX "  // 2
        "   X X  "  // 3
        "  XXXXX "  // 4
        "   X X  "  // 5
        "        "  // 6
        "        "; // 7
    case '$': return
        // 234567
        "        "  // 0
        "   XXX  "  // 1
        "  X X   "  // 2
        "   XXX  "  // 3
        "    X X "  // 4
        "   XXX  "  // 5
        "    X   "  // 6
        "        "; // 7
    case '%': return
        // 234567
        "        "  // 0
        "  XX  X "  // 1
        "  X  X  "  // 2
        "    X   "  // 3
        "   X  X "  // 4
        "  X  XX "  // 5
        "        "  // 6
        "        "; // 7
    case '^': return
        // 234567
        "        "  // 0
        "    X   "  // 1
        "   X X  "  // 2
        "  X   X "  // 3
        "        "  // 4
        "        "  // 5
        "        "  // 6
        "        "; // 7
    case '&': return
        // 234567
        "        "  // 0
        "   XXXX "  // 1
        "  X X   "  // 2
        "   XXX  "  // 3
        "  X X   "  // 4
        "   XXXX "  // 5
        "    X   "  // 6
        "        "; // 7
    case '*': return
        // 234567
        "        "  // 0
        "        "  // 1
        "   XX   "  // 2
        "   XXX  "  // 3
        "   XXX  "  // 4
        "        "  // 5
        "        "  // 6
        "        "; // 7
    case '(': return
        // 234567
        "        "  // 0
        "    X   "  // 1
        "   X    "  // 2
        "   X    "  // 3
        "   X    "  // 4
        "    X   "  // 5
        "        "  // 6
        "        "; // 7
    // ---------------------------- Non-printables ----------------------------
    case U' ': default:
    //    01234567
    return "        "  // 0
           "        "  // 1
           "        "  // 2
           "        "  // 3
           "        "  // 4
           "        "  // 5
           "        "  // 6
           "        "; // 7
    }
}

const char * check_8x8_char(const char * uc) {
    static constexpr const auto k_font_char_size = 8;
    assert(uc);
    assert(strlen(uc) == k_font_char_size*k_font_char_size);
    return uc;
}

} // end of <anonymous> namespace
