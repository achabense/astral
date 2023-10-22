#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <string>
#include <vector>

namespace legacy {
    using std::array;
    using std::string;
    using std::vector;

    // clang-format off
    // The environment around "s".
    struct envT {
        bool q, w, e;
        bool a, s, d;
        bool z, x, c;

        constexpr int encode() const {
            // ~ bool is implicitly promoted to int.
            int code = (q << 0) | (w << 1) | (e << 2) |
                       (a << 3) | (s << 4) | (d << 5) |
                       (z << 6) | (x << 7) | (c << 8);
            assert(code >= 0 && code < 512);
            return code;
        }
    };
    // clang-format on

    constexpr envT decode(int code) {
        assert(code >= 0 && code < 512);
        bool q = (code >> 0) & 1, w = (code >> 1) & 1, e = (code >> 2) & 1;
        bool a = (code >> 3) & 1, s = (code >> 4) & 1, d = (code >> 5) & 1;
        bool z = (code >> 6) & 1, x = (code >> 7) & 1, c = (code >> 8) & 1;
        return {q, w, e, a, s, d, z, x, c};
    }

    // Equivalent to envT(...).encode().
    constexpr int encode(bool q, bool w, bool e, bool a, bool s, bool d, bool z, bool x, bool c) {
        int code = (q << 0) | (w << 1) | (e << 2) | (a << 3) | (s << 4) | (d << 5) | (z << 6) | (x << 7) | (c << 8);
        assert(code >= 0 && code < 512);
        return code;
    }

    // Unambiguously refer to the map from env-code to the new state.
    // TODO: reconsider inheritance-based approach...
    // TODO: should allow implicit conversion?
    // TODO: recheck...

    // TODO: is it suitable to declare a namespace-enum for this?
    enum interpret_mode : bool { ABS, XOR };

    // TODO: make from_base, to_base, global functions?
    struct ruleT : public array<bool, 512> {
        using array_base = array<bool, 512>;

        constexpr ruleT() : array_base{} {}
        // TODO: better documentation, as xor or flip
        // TODO: explain; better logic structures...
        // TODO: from_base???
        constexpr ruleT(const array_base& base, interpret_mode mode) : array_base{base} {
            if (mode == XOR) {
                for (int code = 0; code < 512; ++code) {
                    bool s = (code >> 4) & 1; // TODO: helper-function...
                    operator[](code) = operator[](code) ? !s : s;
                }
            }
        }

        array_base to_base(interpret_mode mode) const {
            array_base r{*this};
            if (mode == XOR) {
                for (int code = 0; code < 512; ++code) {
                    bool s = (code >> 4) & 1; // TODO: helper-function...
                    r[code] = r[code] == s ? false : true;
                }
            }
            return r;
        }

        bool map(int code) const {
            assert(code >= 0 && code < 512);
            return operator[](code);
        }

        bool map(const envT& env) const {
            return map(env.encode());
        }

        bool map(bool q, bool w, bool e, bool a, bool s, bool d, bool z, bool x, bool c) const {
            return map(encode(q, w, e, a, s, d, z, x, c));
        }
    };
} // namespace legacy
