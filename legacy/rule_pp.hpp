#pragma once

#include <map>
#include <random>
#include <span>

#include "rule.hpp"

namespace legacy {
    inline bool will_flick(const ruleT& rule) {
        // TODO: adhoc...
        return rule(codeT{0}) == 1 && rule(codeT{511}) == 0; // TODO: more robust encoding?
    }

    // TODO: rename / explain...
    struct partialT {
        enum stateE : char { S0, S1, Unknown };
        using data_type = codeT::map_to<stateE>;
        data_type map;
        void reset() { map.fill(Unknown); }
        partialT() { reset(); }

#if 0
        // How to deal with map[code] != Unknown && != b?
        void set(codeT code, bool b) { map[code] = stateE{b}; }

        // TODO: sometimes need to listen to selected area?
        auto bind(const ruleT& rule) {
            return [this, rule](codeT code) /*const*/ {
                set(code, rule(code));
                return rule(code);
            };
        }
#endif
    };

    using ruleT_data = ruleT::data_type;

    // Interpretation of ruleT_data.
    // TODO: explain...
    // The data in `ruleT` has fixed meaning - the value `s` is mapped to at next generation.
    // However, this is not the only meaningful way to interpret the data...
    // ...
    struct interT {
        enum tagE : int {
            Value, // The value s is mapped to (the same as in ruleT).
            Flip,  // Is s flipped.
            Diff   // Is the result the same as that of a custom rule.
        };
        tagE tag = Value;
        ruleT custom{};

        const ruleT& get_viewer() const { return get_viewer(tag); }

        const ruleT& get_viewer(tagE tag) const {
            static constexpr ruleT zero{};
            // TODO: mkrule is not  quite useful... remove it finally...
            static constexpr ruleT identity = mkrule(decode_s);

            switch (tag) {
            case Value: return zero;
            case Flip: return identity;
            case Diff: return custom;
            default: abort();
            }
        }

        // both methods are actually XOR...
        // TODO: better name...
        ruleT_data from_rule(const ruleT& rule) const {
            const ruleT& viewer = get_viewer();
            ruleT_data diff{};
            for (codeT code : codeT{}) {
                diff[code] = rule(code) == viewer(code) ? 0 : 1;
            }
            return diff;
        }

        ruleT to_rule(const ruleT_data& diff) const {
            const ruleT& viewer = get_viewer();
            ruleT rule{};
            for (codeT code : codeT{}) {
                rule.map[code] = diff[code] ? !viewer(code) : viewer(code);
            }
            return rule;
        }
    };

} // namespace legacy

namespace legacy {
    // A mapperT defines a rule that maps each codeT to another codeT.
    // Specifically, mapperT{q2=q,w2=w,...} maps any codeT to the same value.
    struct mapperT {
        enum takeE { v0, v1, q, w, e, a, s, d, z, x, c, nq, nw, ne, na, ns, nd, nz, nx, nc };
        takeE q2, w2, e2;
        takeE a2, s2, d2;
        takeE z2, x2, c2;

        constexpr mapperT(std::string_view str) {
            // TODO: assert format ([01]|!?[qweasdzxc]){9}...
            const char *pos = str.data(), *end = pos + str.size();
            auto take2 = [&]() -> takeE {
                assert(pos != end);
                bool neg = false;
                switch (*pos) {
                case '0': ++pos; return v0;
                case '1': ++pos; return v1;
                case '!':
                    ++pos;
                    neg = true;
                    break;
                }
                assert(pos != end);
                switch (*pos++) {
                case 'q': return neg ? nq : q;
                case 'w': return neg ? nw : w;
                case 'e': return neg ? ne : e;
                case 'a': return neg ? na : a;
                case 's': return neg ? ns : s;
                case 'd': return neg ? nd : d;
                case 'z': return neg ? nz : z;
                case 'x': return neg ? nx : x;
                case 'c': return neg ? nc : c;
                default: assert(false); return v0;
                }
            };
            q2 = take2(), w2 = take2(), e2 = take2();
            a2 = take2(), s2 = take2(), d2 = take2();
            z2 = take2(), x2 = take2(), c2 = take2();
        }

        constexpr codeT operator()(codeT code) const {
            const envT env = decode(code);
            const bool qweasdzxc[9]{env.q, env.w, env.e, env.a, env.s, env.d, env.z, env.x, env.c};
            const auto take = [&qweasdzxc](takeE t) -> bool {
                if (t == v0) {
                    return 0;
                } else if (t == v1) {
                    return 1;
                } else if (t >= q && t <= c) {
                    return qweasdzxc[t - q];
                } else {
                    return !qweasdzxc[t - nq];
                }
            };

            // clang-format off
            return encode(take(q2), take(w2), take(e2),
                          take(a2), take(s2), take(d2),
                          take(z2), take(x2), take(c2));
            // clang-format on
        }
    };

    // A pair of mapperT defines an equivalence relation.
    // TODO: explain... how to transfer to rule-concepts?

    // TODO: is this correct(enough)?
    // Conceptual:
    // Efficient rule-checking doesn't actually need intermediate types:
    inline bool test(const ruleT_data& rule, mapperT a, mapperT b) {
        for (codeT c : codeT{}) {
            if (rule[a(c)] != rule[b(c)]) {
                return false;
            }
        }
        return true;
    }

    // TODO: refine "concept"...
    // 1. Does a rule satisfy the concept.
    // 2. How to generate all possible rules that satisfy some concepts...
    inline bool satisfies(const ruleT& rule, const interT& inter, mapperT a, mapperT b) {
        return test(inter.from_rule(rule), a, b);
    }

    // Partition of all codeT ({0}~{511}), in the form of union-find set.
    // (Lacks the ability to efficiently list groups)
    // TODO: explain: a collection of equivalences denoted by mapperT pairs.
    class equivT {
        mutable codeT::map_to<codeT> parof;

    public:
        equivT() {
            for (codeT c : codeT{}) {
                parof[c] = c;
            }
        }

        // TODO: const or not?
        // headof?
        codeT rootof(codeT c) const {
            if (parof[c] == c) {
                return c;
            } else {
                return parof[c] = rootof(parof[c]);
            }
        }

        bool test(const ruleT_data& r) const {
            for (codeT code : codeT{}) {
                if (r[code] != r[parof[code]]) {
                    return false;
                }
            }
            return true;
        }

        void add_eq(codeT a, codeT b) { parof[rootof(a)] = rootof(b); }
        void add_eq(mapperT a, mapperT b) {
            for (codeT c : codeT{}) {
                add_eq(a(c), b(c));
            }
        }
        void add_eq(const equivT& e) {
            for (codeT c : codeT{}) {
                add_eq(c, e.rootof(c));
            }
        }

        bool has_eq(codeT a, codeT b) const { return rootof(a) == rootof(b); }
        bool has_eq(mapperT a, mapperT b) const {
            for (codeT c : codeT{}) {
                if (!has_eq(a(c), b(c))) {
                    return false;
                }
            }
            return true;
        }
        // TODO: can this correctly check refinement-relation?
        bool has_eq(const equivT& e) const {
            for (codeT c : codeT{}) {
                if (!has_eq(c, e.rootof(c))) {
                    return false;
                }
            }
        }
    };

    inline bool satisfies(const ruleT& rule, const interT& inter, const equivT& q) {
        return q.test(inter.from_rule(rule));
    }
} // namespace legacy

namespace legacy::special_mappers {
    // TODO: add common prefix for better auto-completion...
    // TODO: explain...
    inline constexpr mapperT identity("qweasdzxc");

    // Native symmetry.
    // Combination of these requirements can lead to ... TODO: explain, and explain "requirements"...
    inline constexpr mapperT asd_refl("zxc"
                                      "asd"
                                      "qwe"); // '-'
    inline constexpr mapperT wsx_refl("ewq"
                                      "dsa"
                                      "cxz"); // '|'
    inline constexpr mapperT qsc_refl("qaz"
                                      "wsx"
                                      "edc"); // '\'
    inline constexpr mapperT esz_refl("cde"
                                      "xsw"
                                      "zaq"); // '/'
    inline constexpr mapperT ro_180("cxz"
                                    "dsa"
                                    "ewq"); // 180
    inline constexpr mapperT ro_90("zaq"
                                   "xsw"
                                   "cde"); // 90 (clockwise)
    // TODO: does this imply 270 clockwise?

    // TODO: explain; actually irrelevant of symmetry...
    // 1. I misunderstood "rotate" symmetry. "ro45" is never about symmetry (I've no idea what it is)
    // 2. As seemingly-senseless partition like ro45 can make non-trivial patterns, should support after all...
    inline constexpr mapperT ro_45("aqw"
                                   "zse"
                                   "xcd"); // "45" clockwise. TODO: explain...
    // TODO: support that totalistic...

    // TODO: explain. TODO: better name...
    inline constexpr mapperT dual("!q!w!e"
                                  "!a!s!d"
                                  "!z!x!c");

    // TODO: about ignore_s and interT...
    inline constexpr mapperT ignore_q("0weasdzxc");
    inline constexpr mapperT ignore_w("q0easdzxc");
    inline constexpr mapperT ignore_e("qw0asdzxc");
    inline constexpr mapperT ignore_a("qwe0sdzxc");
    inline constexpr mapperT ignore_s("qwea0dzxc");
    inline constexpr mapperT ignore_d("qweas0zxc");
    inline constexpr mapperT ignore_z("qweasd0xc");
    inline constexpr mapperT ignore_x("qweasdz0c");
    inline constexpr mapperT ignore_c("qweasdzx0");

    // Hexagonal emulation and emulated symmetry.

    // qw-     q w
    // asd -> a s d
    // -xc     x c
    // TODO: super problematic, especially refl...
    inline constexpr mapperT hex_ignore("qw0"
                                        "asd"
                                        "0xc"); // ignore_(e, z)
    // TODO: explain why 0 instead of e-z... ... is this really correct?
    inline constexpr mapperT hex_wsx_refl("dw0"
                                          "csq"
                                          "0xa"); // swap q-a and d-c
    inline constexpr mapperT hex_qsc_refl("qa0"
                                          "wsx"
                                          "0dc"); // swap ... TODO: complete...
    inline constexpr mapperT hex_asd_refl("xc0"
                                          "asd"
                                          "0qw"); // swap (q,x) (w,c)

    // TODO: complete...
    // TODO: better name...
    inline constexpr mapperT hex_qwxc_refl("wq0"
                                           "dsa"
                                           "0cx"); // swap(q,w), swap(a,d), swap(x,c)

    inline constexpr mapperT hex_ro_180("cx0"
                                        "dsa"
                                        "0wq"); // 180
    inline constexpr mapperT hex_ro_120("xa0"
                                        "csq"
                                        "0dw"); // 120 (clockwise)
    inline constexpr mapperT hex_ro_60("aq0"
                                       "xsw"
                                       "0cd"); // 60 (clockwise)

    // TODO: support ignore_(q, c) version?

} // namespace legacy::special_mappers

namespace legacy {

    using groupT = std::span<const codeT>;

    // With full capacity...
    class partitionT {
        using indexT = int;
        codeT::map_to<indexT> m_p;
        int m_k;

        // Though with size 512, m_data is not a map for codeT.
        // Instead, it stores codeT of the same group consecutively.
        // (In short, m_data[codeT] has no meaning related to codeT itself...)
        std::array<codeT, 512> m_data;
        struct group_pos {
            int pos, size;
        };
        // TODO: there is std::sample; will be very convenient if storing directly...
        // Pro: directly-copyable.
        // Con: harder to use...
        std::vector<group_pos> m_groups;

    public:
        int k() const { return m_k; }
        groupT jth_group(indexT j) const {
            const auto [pos, size] = m_groups[j];
            return groupT(m_data.data() + pos, size);
        }

        // TODO: whether to expose index?
        indexT index_for(codeT code) const { return m_p[code]; }
        groupT group_for(codeT code) const { return jth_group(index_for(code)); }
        // TODO: when is head for needed?
        // TODO: why not directly store a equivT?
        codeT head_for(codeT code) const {
            // group_for(code).front();
            return m_data[m_groups[index_for(code)].pos];
        }

        /*implicit*/ partitionT(const equivT& u) {
            std::map<codeT, indexT> m;
            indexT i = 0;
            for (codeT c : codeT{}) {
                codeT head = u.rootof(c);
                if (!m.contains(head)) {
                    m[head] = i++;
                }
                m_p[c] = m[head];
            }

            m_k = i;

            std::vector<int> count(m_k, 0);
            for (indexT col : m_p) {
                ++count[col];
            }

            std::vector<int> pos(m_k, 0);
            for (int j = 1; j < m_k; ++j) {
                pos[j] = pos[j - 1] + count[j - 1];
            }

            m_groups.resize(m_k);
            for (int j = 0; j < m_k; ++j) {
                m_groups[j] = {pos[j], count[j]};
            }

            for (codeT code : codeT{}) {
                indexT col = m_p[code];
                m_data[pos[col]++] = code;
            }
        }
        // TODO: Is this needed?
        bool test(const ruleT_data& r) const {
            for (codeT code : codeT{}) {
                if (r[code] != r[head_for(code)]) {
                    return false;
                }
            }
            return true;
        }
        // TODO: is refinement checking needed for partitionT?
        // TODO: refer to https://en.wikipedia.org/wiki/Partition_of_a_set#Refinement_of_partitions
    };

    // TODO: but where does inter come from?
    inline bool satisfies(const ruleT& rule, const interT& inter, const partitionT& par) {
        return par.test(inter.from_rule(rule));
    }

    class scanlistT {
    public:
        enum scanE : char { A0, A1, Inconsistent };

    private:
        // NOTICE: not a map-type. It's just that 512 will be always enough space.
        // TODO: use vector instead?
        std::array<scanE, 512> m_data;
        int m_k;
        int m_count[3];

    public:
        static scanE scan(std::span<const codeT> group, const ruleT_data& rule) {
            bool has[2]{};
            for (codeT code : group) {
                if (has[!rule[code]]) {
                    return Inconsistent;
                }
                has[rule[code]] = true;
            }
            return has[0] ? A0 : A1;
        }

        scanlistT(const partitionT& p, const ruleT_data& rule) : m_data{}, m_k{p.k()}, m_count{} {
            for (int j = 0; j < m_k; ++j) {
                m_data[j] = scan(p.jth_group(j), rule);
                ++m_count[m_data[j]];
            }
        }

        auto begin() const { return m_data.cbegin(); }
        auto end() const { return m_data.cbegin() + m_k; }
        const scanE& operator[](int j) const { return m_data[j]; }

        int k() const { return m_k; }
        int count(scanE s) const { return m_count[s]; }
    };

    // TODO: remove that in main.cpp
    inline void flip(groupT group, ruleT& rule) {
        for (codeT c : group) {
            rule.map[c] = !rule.map[c];
        }
    }
    // TODO: preconditions...
    // void random_flip(const interT& inter, const partitionT& par, const partialT& constraints, int count);
    inline ruleT random_flip(const interT& inter, const partitionT& par, int count_min, int count_max) {
        static std::mt19937 rand(time(0));
        ruleT r = inter.get_viewer();
        // flip some...
        std::vector<groupT> gs;
        for (int j = 0; j < par.k(); ++j) {
            gs.push_back(par.jth_group(j));
        }
        std::vector<groupT> ds;
        std::sample(gs.begin(), gs.end(), std::back_inserter(ds), count_min, rand); // max is not supported...
        for (auto group : ds) {
            flip(group, r);
        }
        return r;
    }

    // TODO: preconditions...
    inline ruleT next_v(const interT& inter, const partitionT& par, const ruleT& r) {}
    inline ruleT prev_v(const interT& inter, const partitionT& par, const ruleT& r) {}
    inline ruleT next_perm(const interT& inter, const partitionT& par, const ruleT& r) {}
    inline ruleT prev_perm(const interT& inter, const partitionT& par, const ruleT& r) {}

    // TODO: example. should not be used this way...
    inline equivT make_eq(std::vector<std::pair<mapperT, mapperT>> eqs) {
        equivT eq{};
        for (const auto& [a, b] : eqs) {
            eq.add_eq(a, b);
        }
        return eq;
    }
    inline equivT make_eq_internal(std::vector<mapperT> eqs) {
        equivT eq{};
        for (const auto& e : eqs) {
            eq.add_eq(e, special_mappers::identity);
        }
        return eq;
    }
    inline const partitionT& sample_partition() {
        using namespace special_mappers;
        // static const partitionT p(make_eq_internal({asd_refl, wsx_refl, qsc_refl, esz_refl, ro_90}));
        static const partitionT p(make_eq_internal({hex_ro_60, hex_asd_refl, hex_ignore}));

        return p;
    }

} // namespace legacy
