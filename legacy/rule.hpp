#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <regex>
#include <string>
#include <vector>

// TODO: tell apart precondition and impl assertion...

// TODO: remove the `T` suffix? initially they were for things like `codeT code`...
// TODO: bool might not be a good idea...
// For example, bool is allowed to have padding bits, so memcmp may not apply...

namespace legacy {
    // The environment around "s".
    // TODO: other layouts are possible: https://en.wikipedia.org/wiki/QWERTZ
    struct envT {
        bool q, w, e;
        bool a, s, d;
        bool z, x, c;
    };

    // TODO: remove remaining direct use of "512"...
    struct codeT {
        int val;
        constexpr /*implicit*/ operator int() const {
            assert(val >= 0 && val < 512);
            return val;
        }

        template <class T>
        using map_to = std::array<T, 512>;

        // rename to bpos_*?
        enum bposE : int { env_q = 0, env_w, env_e, env_a, env_s, env_d, env_z, env_x, env_c };
    };

    // (`name` must not be modified within the loop body)
#define for_each_code(name) for (::legacy::codeT name{.val = 0}; name.val < 512; ++name.val)

    constexpr codeT encode(const envT& env) {
        // ~ bool is implicitly promoted to int.
        // clang-format off
        using enum codeT::bposE;
        const int code = (env.q << env_q) | (env.w << env_w) | (env.e << env_e) |
                         (env.a << env_a) | (env.s << env_s) | (env.d << env_d) |
                         (env.z << env_z) | (env.x << env_x) | (env.c << env_c);
        // clang-format on
        assert(code >= 0 && code < 512);
        return codeT{code};
    }

    constexpr envT decode(codeT code) {
        using enum codeT::bposE;
        const bool q = (code >> env_q) & 1, w = (code >> env_w) & 1, e = (code >> env_e) & 1;
        const bool a = (code >> env_a) & 1, s = (code >> env_s) & 1, d = (code >> env_d) & 1;
        const bool z = (code >> env_z) & 1, x = (code >> env_x) & 1, c = (code >> env_c) & 1;
        return {q, w, e, a, s, d, z, x, c};
    }

    constexpr bool get(codeT code, codeT::bposE bpos) {
        return (code >> bpos) & 1;
    }

    constexpr bool get_s(codeT code) {
        return (code >> codeT::env_s) & 1;
    }

#ifndef NDEBUG
    namespace _misc {
        // TODO: whether to add / keep these tests?
        // (full tests covering the whole project is infeasible...)
        inline const bool test_codeT = [] {
            for_each_code(code) {
                assert(encode(decode(code)) == code);
            }
            return true;
        }();
    } // namespace _misc
#endif

    // TODO: rephrase...
    // Unambiguously refer to the map from env-code to the new state.
    class ruleT {
        codeT::map_to<bool> m_map{};

    public:
        // (TODO; temp) compared to operator[]:
        // pro: will not expose the address...
        // con: less "natural" than operator[] (especially as std::*map etc uses [] too)
        // constexpr void set(codeT code, bool b) { m_map[code] = b; }

        // TODO: explain why defining an extra operator().
        constexpr bool operator()(codeT code) const { return m_map[code]; }

        constexpr bool operator[](codeT code) const { return m_map[code]; }
        constexpr bool& operator[](codeT code) { return m_map[code]; }

        constexpr friend bool operator==(const ruleT&, const ruleT&) = default;
    };

    // "Convay's Game of Life" (B3/S23)
    inline ruleT game_of_life() {
        ruleT rule{};
        for_each_code(code) {
            const auto [q, w, e, a, s, d, z, x, c] = decode(code);
            const int count = q + w + e + a + d + z + x + c;
            if (count == 2) { // 2:S ~ 0->0, 1->1 ~ equal to "s".
                rule[code] = s;
            } else if (count == 3) { // 3:BS ~ 0->1, 1->1 ~ always 1.
                rule[code] = 1;
            } else {
                rule[code] = 0;
            }
        }
        return rule;
    }

    class compressT {
        std::array<uint8_t, 64> bits; // as bitset.
    public:
        explicit compressT(const ruleT& rule) : bits{} {
            for_each_code(code) {
                bits[code / 8] |= rule[code] << (code % 8);
            }
        }

        /*implicit*/ operator ruleT() const {
            ruleT rule{};
            for_each_code(code) {
                rule[code] = (bits[code / 8] >> (code % 8)) & 1;
            }
            return rule;
        }

        friend bool operator==(const compressT& l, const compressT& r) = default;

        struct hashT {
            size_t operator()(const compressT& cmpr) const {
                // ~ not UB.
                const char* data = reinterpret_cast<const char*>(cmpr.bits.data());
                return std::hash<std::string_view>{}(std::string_view(data, 64));
            }
        };
    };

    namespace _misc {
        inline char to_base64(uint8_t b6) {
            if (b6 < 26) {
                return 'A' + b6;
            } else if (b6 < 52) {
                return 'a' + b6 - 26;
            } else if (b6 < 62) {
                return '0' + b6 - 52;
            } else if (b6 == 62) {
                return '+';
            } else {
                assert(b6 == 63);
                return '/';
            }
        }

        inline uint8_t from_base64(char ch) {
            if (ch >= 'A' && ch <= 'Z') {
                return ch - 'A';
            } else if (ch >= 'a' && ch <= 'z') {
                return 26 + ch - 'a';
            } else if (ch >= '0' && ch <= '9') {
                return 52 + ch - '0';
            } else if (ch == '+') {
                return 62;
            } else {
                assert(ch == '/');
                return 63;
            }
        }
    } // namespace _misc

    // Convert ruleT to an "MAP rule" string.
    // TODO: recheck... is `MAP_rule` a suitable name?
    inline std::string to_MAP_str(const ruleT& rule) {
        // MAP rule uses a different coding scheme.
        bool MAP_rule[512]{};
        for_each_code(code) {
            const auto [q, w, e, a, s, d, z, x, c] = decode(code);
            MAP_rule[q * 256 + w * 128 + e * 64 + a * 32 + s * 16 + d * 8 + z * 4 + x * 2 + c * 1] = rule[code];
        }
        const auto get = [&MAP_rule](int i) { return i < 512 ? MAP_rule[i] : 0; };
        std::string str = "MAP";
        for (int i = 0; i < 512; i += 6) {
            const uint8_t b6 = (get(i + 5) << 0) | (get(i + 4) << 1) | (get(i + 3) << 2) | (get(i + 2) << 3) |
                               (get(i + 1) << 4) | (get(i + 0) << 5);
            str += _misc::to_base64(b6);
        }
        return str;
    }

    inline const std::regex& regex_MAP_str() {
        static_assert((512 + 5) / 6 == 86);
        static const std::regex reg{"MAP[a-zA-Z0-9+/]{86}", std::regex_constants::optimize};
        return reg;
    }

    inline ruleT from_MAP_str(const std::string& map_str) {
        assert(std::regex_match(map_str, regex_MAP_str()));
        bool MAP_rule[512]{};
        auto put = [&MAP_rule](int i, bool b) {
            if (i < 512) {
                MAP_rule[i] = b;
            }
        };

        int chp = 3; // skip "MAP"
        for (int i = 0; i < 512; i += 6) {
            const uint8_t b6 = _misc::from_base64(map_str[chp++]);
            put(i + 5, (b6 >> 0) & 1);
            put(i + 4, (b6 >> 1) & 1);
            put(i + 3, (b6 >> 2) & 1);
            put(i + 2, (b6 >> 3) & 1);
            put(i + 1, (b6 >> 4) & 1);
            put(i + 0, (b6 >> 5) & 1);
        }
        ruleT rule{};
        for_each_code(code) {
            const auto [q, w, e, a, s, d, z, x, c] = decode(code);
            rule[code] = MAP_rule[q * 256 + w * 128 + e * 64 + a * 32 + s * 16 + d * 8 + z * 4 + x * 2 + c * 1];
        }
        return rule;
    }

#ifndef NDEBUG
    namespace _misc {
        // https://golly.sourceforge.io/Help/Algorithms/QuickLife.html
        // > So, Conway's Life (B3/S23) encoded as a MAP rule is:
        // > rule = MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA
        inline const bool test_MAP_str = [] {
            const char* gol_str =
                "MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA";
            const ruleT gol = game_of_life();
            assert(std::regex_match(gol_str, regex_MAP_str()));
            assert(gol_str == to_MAP_str(gol));
            assert(from_MAP_str(gol_str) == gol);
            return true;
        }();
    } // namespace _misc
#endif

} // namespace legacy
