#pragma once

#include "rule.hpp"

namespace legacy {
    // TODO: better name for ruleT_masked?
    // TODO: is it safe to define maskT this way?
    // TODO: explain the meaning of maskT_result (how is a rule different from maskT)...

    // TODO: explain these types are defined only to avoid concept misuse...
    // A maskT is a special ruleT used to do XOR mask for other rules.
    struct maskT : public ruleT {};
    using ruleT_masked = codeT::map_to<bool>;

    inline void operator^(const maskT&, const maskT&) = delete;

    inline ruleT_masked operator^(const maskT& mask, const ruleT& rule) {
        ruleT_masked r{};
        for_each_code([&](codeT code) { r[code] = mask[code] ^ rule[code]; });
        return r;
    }

    inline ruleT operator^(const maskT& mask, const ruleT_masked& r) {
        ruleT rule{};
        for_each_code([&](codeT code) { rule[code] = mask[code] ^ r[code]; });
        return rule;
    }

    // TODO: rephrase...
    // Equivalence relation for codeT ({0...511}), in the form of union-find set.
    class equivT {
        friend class partitionT;

        mutable codeT::map_to<codeT> parof;

        codeT headof(codeT c) const {
            if (parof[c] == c) {
                return c;
            } else {
                return parof[c] = headof(parof[c]);
            }
        }

    public:
        equivT() {
            for_each_code([&](codeT code) { parof[code] = code; });
        }

        bool test(const ruleT_masked& r) const {
            return for_each_code_all_of([&](codeT code) { //
                return r[code] == r[parof[code]];
            });
        }

        bool test(const ruleT_masked& r, const moldT::lockT& lock) const {
            codeT::map_to<int> record;
            record.fill(-1);

            return for_each_code_all_of([&](codeT code) {
                if (!lock[code]) {
                    return true;
                }

                int& rep = record[headof(code)];
                if (rep == -1) {
                    rep = r[code];
                }
                return rep == r[code];
            });
        }

        // TODO: better names...
        void add_eq(codeT a, codeT b) { parof[headof(a)] = headof(b); }
        void add_eq(const equivT& other) {
            for_each_code([&](codeT code) { add_eq(code, other.parof[code]); });
        }

        bool has_eq(codeT a, codeT b) const { return headof(a) == headof(b); }
        bool has_eq(const equivT& other) const {
            return for_each_code_all_of([&](codeT code) { //
                return has_eq(code, other.parof[code]);
            });
        }
    };

    using groupT = std::span<const codeT>;

    class partitionT {
        equivT m_eq;

        // TODO: explain more naturally...
        // Map codeT to [j: its group is the jth one] encountered during the `for_each_code` scan.
        codeT::map_to<int> m_map;
        int m_k;

        // TODO: (temp) without {} the compiler will give warnings;
        // (It seems the compiler is unable to detect that m_data[0...511] is fully assigned...)

        // `m_data` is not a codeT::map_to<codeT>.
        // It's for storing codeT of the same group consecutively.
        std::array<codeT, 512> m_data{};
        struct group_pos {
            int pos, size;
        };
        std::vector<group_pos> m_groups{};

        groupT jth_group(int j) const {
            const auto [pos, size] = m_groups[j];
            return groupT(m_data.data() + pos, size);
        }

    public:
        groupT group_for(codeT code) const { return jth_group(m_map[code]); }

        int k() const { return m_k; }
        void for_each_group(const auto& fn) const {
            for (int j = 0; j < m_k; ++j) {
                if constexpr (requires { fn(jth_group(j)); }) {
                    fn(jth_group(j));
                } else {
                    fn(j, jth_group(j));
                }
            }
        }

        // TODO: (temp) these "user-declared" ctors are enough to avoid implicit moving...
        // Avoid moving `m_data`:
        partitionT(const partitionT&) = default;
        partitionT& operator=(const partitionT&) = default;

        /*implicit*/ partitionT(const equivT& u) : m_eq(u) {
            m_k = 0;

            codeT::map_to<int> m;
            m.fill(-1);
            for_each_code([&](codeT code) {
                const codeT head = u.headof(code);
                if (m[head] == -1) {
                    m[head] = m_k++;
                }
                m_map[code] = m[head];
            });
            // m_k is now the number of groups in the partition.

            std::vector<int> count(m_k, 0);
            for_each_code([&](codeT code) { ++count[m_map[code]]; });

            std::vector<int> pos(m_k, 0);
            for (int j = 1; j < m_k; ++j) {
                pos[j] = pos[j - 1] + count[j - 1];
            }

            m_groups.resize(m_k);
            for (int j = 0; j < m_k; ++j) {
                m_groups[j] = {pos[j], count[j]};
            }

            for_each_code([&](codeT code) {
                int j = m_map[code];
                m_data[pos[j]++] = code;
            });
        }

        bool test(const ruleT_masked& r) const { return m_eq.test(r); }
        bool test(const ruleT_masked& r, const moldT::lockT& lock) const { return m_eq.test(r, lock); }

        bool is_refinement_of(const partitionT& other) const { return other.m_eq.has_eq(m_eq); }

        friend partitionT operator|(const partitionT& a, const partitionT& b) {
            equivT ab = a.m_eq;
            ab.add_eq(b.m_eq);
            return partitionT(ab);
        }
    };

    // A subsetT defines a subset of all possible ruleT.
    // TODO: lockT is currently not a part of subsetT (but do take part in generation)
    // Extension is possible - let subsetT be ...(TODO, detailed explanation)
    class subsetT {
        struct nonemptyT {
            maskT mask;
            partitionT par;

            bool contains(const ruleT& rule) const { return par.test(mask ^ rule); }
            bool includes(const nonemptyT& other) const {
                return contains(other.mask) && par.is_refinement_of(other.par);
            }
        };

        std::optional<nonemptyT> m_set;

    public:
        subsetT(const maskT& mask, const equivT& eq) { m_set.emplace(mask, eq); }
        subsetT(const maskT& mask, const partitionT& par) { m_set.emplace(mask, par); }

        // TODO: replace with static ctor... static subsetT XXX();
        struct emptyT {};
        subsetT(emptyT) {}
        struct universalT {};
        subsetT(universalT) : subsetT(maskT{}, equivT{}) {}

        bool empty() const { return !m_set.has_value(); }
        bool contains(const ruleT& rule) const { return m_set && m_set->contains(rule); }
        bool includes(const subsetT& other) const { return other.empty() || (m_set && m_set->includes(*other.m_set)); }
        bool equals(const subsetT& other) const { return includes(other) && other.includes(*this); }

        // TODO: is !empty() precond or runtime-error?
        const maskT& get_mask() const {
            assert(!empty());
            return m_set->mask;
        }
        const partitionT& get_par() const {
            assert(!empty());
            return m_set->par;
        }
        // TODO: whether to change the subset's mask directly? or just use the an independent mask
        // under the control of the subset?
        bool set_mask(const maskT& mask) {
            assert(!empty());
            if (!contains(mask)) {
                return false;
            }
            m_set->mask = mask;
            return true;
        }

        // TODO: finish the proof...
        // Prove that the intersection (&) of any two subsetT (a) and (b) is still another subsetT.
        // If (a & b) results in an empty set, it is a subsetT.
        // Otherwise, let (p) be (a.par | b.par), and let (r) be one of the rule that belongs to (a & b).
        // It can be shown that (a & b) == subsetT(r, p).
        // 1. Any rule in (r, p) can be gotten by flipping some groups of (p) from (r).
        // ...
        // 2. For any r' != r but belongs to (a) and (b), ... look at any [codeT] of it.
        // ...
        friend subsetT operator&(const subsetT& a_, const subsetT& b_) {
            if (a_.empty() || b_.empty()) {
                return {emptyT{}};
            }

            const nonemptyT &a = *a_.m_set, &b = *b_.m_set;
            partitionT par_both = a.par | b.par;

            if (a.contains(b.mask)) {
                return {b.mask, par_both};
            } else if (b.contains(a.mask)) {
                return {a.mask, par_both};
            }

            // Look for a rule that both a and b contains.
            ruleT common_rule{};
            codeT::map_to<bool> assigned{};

            // TODO: explain try-assign will result a correct rule iff a & b != {}.
            // TODO: analyze complexity...
            auto try_assign = [&](const codeT code, const bool v, auto& self) -> void {
                if (!assigned[code]) {
                    assigned[code] = true;
                    common_rule[code] = v;
                    const bool masked_by_a = a.mask[code] ^ v;
                    const bool masked_by_b = b.mask[code] ^ v;
                    for (const codeT c : a.par.group_for(code)) {
                        self(c, a.mask[c] ^ masked_by_a, self);
                    }
                    for (const codeT c : b.par.group_for(code)) {
                        self(c, b.mask[c] ^ masked_by_b, self);
                    }
                }
            };

            // TODO: this is really slow in debug mode...
            for_each_code([&](codeT code) {
                if (!assigned[code]) {
                    assert(std::ranges::none_of(par_both.group_for(code), [&](codeT c) { return assigned[c]; }));
                    try_assign(code, 0, try_assign);
                    assert(std::ranges::all_of(par_both.group_for(code), [&](codeT c) { return assigned[c]; }));
                }
            });

            if (!a.contains(common_rule) || !b.contains(common_rule)) {
                return {emptyT{}};
            }
            return {{common_rule}, par_both};
        }
    };

#if 0
    // TODO: apply scanlist-based strategy?
    class scanT {
        struct counterT {
            // 0/1 means masked value.
            int free_0 = 0, free_1 = 0;
            int lock_0 = 0, lock_1 = 0;

            bool any_locked() const { return lock_0 || lock_1; }
            bool all_locked() const { return !free_0 && !free_1; }
            bool none_locked() const { return !any_locked(); }
        };

        std::vector<counterT> vec;

    public:
        scanT(const subsetT& subset, const moldT& mold) {
            const maskT& mask = subset.get_mask();
            const partitionT& par = subset.get_par();
            const ruleT_masked r = mask ^ mold.rule;

            vec.resize(par.k());
            par.for_each_group([&](int j, const groupT& group) {
                for (codeT code : group) {
                    if (mold.lock[code]) {
                        r[code] ? ++vec[j].lock_1 : ++vec[j].lock_0;
                    } else {
                        r[code] ? ++vec[j].free_1 : ++vec[j].free_0;
                    }
                }
            });
        }

        int count_free() const { return std::ranges::count_if(vec, &counterT::none_locked); }
    };
#endif

    // TODO: redesign param...
    inline auto scan(const partitionT& par, const ruleT_masked& rule, const moldT::lockT& lock) {
        struct counterT {
            int free_0 = 0, free_1 = 0;
            int locked_0 = 0, locked_1 = 0;

            bool any_locked() const { return locked_0 || locked_1; }
            bool none_locked() const { return !any_locked(); }
            bool all_locked() const { return !free_0 && !free_1; }

            bool all_0() const { return !free_1 && !locked_1; }
            bool all_1() const { return !free_0 && !locked_0; }
            bool inconsistent() const { return !all_0() && !all_1(); }
        };

        std::vector<counterT> vec(par.k());
        par.for_each_group([&](int j, const groupT& group) {
            for (codeT code : group) {
                if (lock[code]) {
                    rule[code] ? ++vec[j].locked_1 : ++vec[j].locked_0;
                } else {
                    rule[code] ? ++vec[j].free_1 : ++vec[j].free_0;
                }
            }
        });
        return vec;
    }

    inline bool any_locked(const moldT::lockT& lock, const groupT& group) {
        return std::ranges::any_of(group, [&lock](codeT code) { return lock[code]; });
    }

    inline bool all_locked(const moldT::lockT& lock, const groupT& group) {
        return std::ranges::all_of(group, [&lock](codeT code) { return lock[code]; });
    }

    inline bool none_locked(const moldT::lockT& lock, const groupT& group) {
        return std::ranges::none_of(group, [&lock](codeT code) { return lock[code]; });
    }

    // TODO: used in `edit_rule`; avoidable if scan early...
    inline int count_free(const partitionT& par, const moldT::lockT& lock) {
        int free = 0;
        par.for_each_group([&](const groupT& group) {
            if (none_locked(lock, group)) {
                ++free;
            }
        });
        return free;
    }

    // TODO: too strict?
    // TODO: add nodiscard? the function name looks like in-place modification...
    // or redesign params...
    inline moldT::lockT enhance_lock(const subsetT& subset, const moldT& mold) {
        assert(subset.contains(mold.rule));

        moldT::lockT lock{};
        subset.get_par().for_each_group([&](const groupT& group) {
            if (any_locked(mold.lock, group)) {
                for (codeT code : group) {
                    lock[code] = true;
                }
            }
        });
        return lock;
    }

    // TODO: (temp) experimental... may consider the "forging mode" approach finally.
    // Notice this is different from flipping the values of each lock[code].
    inline moldT::lockT invert_lock(const subsetT& subset, const moldT& mold) {
        assert(subset.contains(mold.rule));

        moldT::lockT lock{};
        subset.get_par().for_each_group([&](const groupT& group) {
            if (none_locked(mold.lock, group)) {
                for (codeT code : group) {
                    lock[code] = true;
                }
            }
        });
        return lock;
    }

    inline bool compatible(const subsetT& subset, const moldT& mold) {
        if (subset.empty()) {
            return false;
        }

        const ruleT_masked r = subset.get_mask() ^ mold.rule;
        return subset.get_par().test(r, mold.lock);
    }

    inline ruleT approximate(const subsetT& subset, const moldT& mold) {
        assert(compatible(subset, mold));

        const maskT& mask = subset.get_mask();
        const partitionT& par = subset.get_par();

        ruleT_masked r = mask ^ mold.rule;
        par.for_each_group([&](const groupT& group) {
            const auto fnd = std::ranges::find_if(group, [&](codeT code) { return mold.lock[code]; });
            const bool b = fnd != group.end() ? r[*fnd] : r[group[0]];
            for (codeT code : group) {
                r[code] = b;
            }
        });

        // TODO: explain...
        assert(!subset.contains(mold.rule) || (mold.rule == (mask ^ r)));
        return mask ^ r;
    }

    // TODO: better name...
    // Also, it might be helpful to support "in-lock" forging. For example, to dial to find potentially related
    // patterns...
    // Directly invert the locks, or add a flag in `forge`? (TODO: recheck `invert_lock`)
    inline ruleT forge_rule(const subsetT& subset, const moldT& mold, std::invocable<bool*, bool*> auto fn) {
        assert(compatible(subset, mold));

        const maskT& mask = subset.get_mask();
        const partitionT& par = subset.get_par();

        ruleT_masked r = mask ^ approximate(subset, mold);

        // `seq` is not a codeT::map_to<bool>.
        assert(par.k() <= 512);
        std::array<bool, 512> seq{};
        int z = 0;
        par.for_each_group([&](const groupT& group) {
            if (none_locked(mold.lock, group)) {
                seq[z] = r[group[0]];
                ++z;
            }
        });

        fn(seq.data(), seq.data() + z);

        z = 0;
        par.for_each_group([&](const groupT& group) {
            if (none_locked(mold.lock, group)) {
                for (codeT code : group) {
                    r[code] = seq[z];
                }
                ++z;
            }
        });

        return mask ^ r;
    }

    // TODO: `count` denotes free groups now; whether to switch to total groups (at least in the gui part)?
    // TODO: (temp) there was a plan to support count_min~count_max mode finally... dropped now.
    inline ruleT randomize(const subsetT& subset, const moldT& mold, std::mt19937& rand, int count) {
        assert(compatible(subset, mold));

        return forge_rule(subset, mold, [&rand, count](bool* begin, bool* end) {
            int c = std::clamp(count, 0, int(end - begin));
            std::fill(begin, end, 0);
            std::fill_n(begin, c, 1);
            std::shuffle(begin, end, rand);
        });
    }

    inline ruleT randomize_v2(const subsetT& subset, const moldT& mold, std::mt19937& rand, double density) {
        assert(compatible(subset, mold));

        return forge_rule(subset, mold, [&rand, density](bool* begin, bool* end) {
            std::bernoulli_distribution dist(std::clamp(density, 0.0, 1.0));
            std::generate(begin, end, [&] { return dist(rand); });
        });
    }

    inline ruleT shuffle(const subsetT& subset, const moldT& mold, std::mt19937& rand) {
        assert(subset.contains(mold.rule));

        return forge_rule(subset, mold, [&rand](bool* begin, bool* end) { std::shuffle(begin, end, rand); });
    }

    // TODO: rename to [set_]first / ...
    struct act_int {
        static ruleT first(const subsetT& subset, const moldT& mold) {
            assert(compatible(subset, mold));

            return forge_rule(subset, mold, [](bool* begin, bool* end) { std::fill(begin, end, 0); });
        }

        static ruleT last(const subsetT& subset, const moldT& mold) {
            assert(compatible(subset, mold));

            return forge_rule(subset, mold, [](bool* begin, bool* end) { std::fill(begin, end, 1); });
        }

        // TODO: next/prev should require `contains`... using compatible to make the gui part work.
        static ruleT next(const subsetT& subset, const moldT& mold) {
            // assert(subset.contains(mold.rule));
            assert(compatible(subset, mold));

            return forge_rule(subset, mold, [](bool* begin, bool* end) {
                // 11...0 -> 00...1; stop at 111...1 (last())
                bool* first_0 = std::find(begin, end, 0);
                if (first_0 != end) {
                    std::fill(begin, first_0, 0);
                    *first_0 = 1;
                }
            });
        }

        static ruleT prev(const subsetT& subset, const moldT& mold) {
            // assert(subset.contains(mold.rule));
            assert(compatible(subset, mold));

            return forge_rule(subset, mold, [](bool* begin, bool* end) {
                // 00...1 -> 11...0; stop at 000...0 (first())
                bool* first_1 = std::find(begin, end, 1);
                if (first_1 != end) {
                    std::fill(begin, first_1, 1);
                    *first_1 = 0;
                }
            });
        }
    };

    // TODO: ++/--count when reaching end?
    // Intentionally using reverse_iterator... TODO: explain why...
    // (TODO: rephrase) As to CTAD vs make_XXX..., here is pitfall for using std::reverse_iterator directly.
    // https://quuxplusone.github.io/blog/2022/08/02/reverse-iterator-ctad/
    struct act_perm {
        // TODO: should there be "set first c to be 1 and rest to be 0" util in the gui?

        static ruleT first(const subsetT& subset, const moldT& mold) {
            assert(subset.contains(mold.rule));

            return forge_rule(subset, mold, [](bool* begin, bool* end) {
                int c = std::count(begin, end, 1);
                std::fill(begin, end, 0);
                std::fill_n(begin, c, 1);
            });
        }

        static ruleT last(const subsetT& subset, const moldT& mold) {
            assert(subset.contains(mold.rule));

            return forge_rule(subset, mold, [](bool* begin, bool* end) {
                int c = std::count(begin, end, 1);
                std::fill(begin, end, 0);
                std::fill_n(end - c, c, 1);
            });
        }

        static ruleT next(const subsetT& subset, const moldT& mold) {
            assert(subset.contains(mold.rule));

            return forge_rule(subset, mold, [](bool* begin, bool* end) {
                // Stop at 000...111 (last())
                if (std::find(std::find(begin, end, 1), end, 0) == end) {
                    return;
                }

                std::next_permutation(std::make_reverse_iterator(end), std::make_reverse_iterator(begin));
            });
        }

        static ruleT prev(const subsetT& subset, const moldT& mold) {
            assert(subset.contains(mold.rule));

            return forge_rule(subset, mold, [](bool* begin, bool* end) {
                // Stop at 111...000 (first())
                if (std::find(std::find(begin, end, 0), end, 1) == end) {
                    return;
                }

                std::prev_permutation(std::make_reverse_iterator(end), std::make_reverse_iterator(begin));
            });
        }
    };

    // TODO: support other transformations (lr/ud etc)?
    // TODO: proper name...
    inline moldT mirror(const moldT& mold) {
        moldT mir{};
        for_each_code([&](codeT code) {
            constexpr codeT::bposE S = codeT::bpos_s;
            const codeT codex = codeT(~code & 511);
            const bool flip = codex.get(S) != mold.rule[codex];
            mir.rule[code] = flip ? !code.get(S) : code.get(S);
            mir.lock[code] = mold.lock[codex];
        });
        return mir;
    }

    inline namespace _make_subset {
        // TODO: better name ("make_mask" is too-general name)...
        inline maskT make_mask(codeT::bposE bpos) {
            return {make_rule([bpos](codeT code) { return code.get(bpos); })};
        }

        // TODO: explain the effects of these mask...
        // rule ^ mask_zero -> TODO
        inline const maskT mask_zero{{}};
        // rule ^ mask_identity -> TODO
        inline const maskT mask_identity{make_mask(codeT::bpos_s)};
        // TODO: mask_copy_q/w/e/a/s(~mask_identity)/d/z/x/c etc?

        // A mapperT defines a rule that maps each codeT to another codeT.
        // Specifically, mapperT{"qweasdzxc"} maps any codeT to the same value.
        class mapperT {
            struct takeT {
                enum tagE { O, I, Get, NGet };
                tagE tag;
                codeT::bposE bpos;
                bool operator()(codeT code) const {
                    switch (tag) {
                        case O: return 0;
                        case I: return 1;
                        case Get: return code.get(bpos);
                        default: assert(tag == NGet); return !code.get(bpos);
                    }
                }
            };

            // TODO: better name...
            takeT q, w, e;
            takeT a, s, d;
            takeT z, x, c;

        public:
            // TODO: about consteval and the (obsolete) plan to support user-defined mappers / subsets...
            consteval mapperT(const char* str) {
                // [01]|!?[qweasdzxc]
                auto parse = [&]() -> takeT {
                    takeT::tagE tag = takeT::Get;
                    switch (*str) {
                        case '0': ++str; return {takeT::O, {}};
                        case '1': ++str; return {takeT::I, {}};
                        case '!':
                            ++str;
                            tag = takeT::NGet;
                            break;
                    }
                    switch (*str++) {
                        case 'q': return {tag, codeT::bpos_q};
                        case 'w': return {tag, codeT::bpos_w};
                        case 'e': return {tag, codeT::bpos_e};
                        case 'a': return {tag, codeT::bpos_a};
                        case 's': return {tag, codeT::bpos_s};
                        case 'd': return {tag, codeT::bpos_d};
                        case 'z': return {tag, codeT::bpos_z};
                        case 'x': return {tag, codeT::bpos_x};
                        case 'c': return {tag, codeT::bpos_c};
                        default: throw 0;
                    }
                };
                q = parse(), w = parse(), e = parse();
                a = parse(), s = parse(), d = parse();
                z = parse(), x = parse(), c = parse();
                if (*str != '\0') {
                    throw 0;
                }
            }

            codeT operator()(codeT code) const {
                return encode({q(code), w(code), e(code), //
                               a(code), s(code), d(code), //
                               z(code), x(code), c(code)});
            }
        };

        // A pair of mapperT defines an equivalence relation.
        struct mapperT_pair {
            mapperT a, b;
        };

        inline void add_eq(equivT& eq, const mapperT_pair& mp) {
            for_each_code([&](codeT code) { eq.add_eq(mp.a(code), mp.b(code)); });
        }

        // TODO: recheck these mappers...

        // mp_identity(any code) -> the same code
        inline constexpr mapperT mp_identity("qweasdzxc");

        // The following mappers are defined relative to mp_identity.
        // That is, the effects actually means the effects of mapperT_pair{mp_identity, mp_*}.

        // TODO: add descriptions...
        // TODO: about ignore_s and maskT...
        inline constexpr mapperT mp_ignore_q("0weasdzxc");
        inline constexpr mapperT mp_ignore_w("q0easdzxc");
        inline constexpr mapperT mp_ignore_e("qw0asdzxc");
        inline constexpr mapperT mp_ignore_a("qwe0sdzxc");
        inline constexpr mapperT mp_ignore_s("qwea0dzxc");
        inline constexpr mapperT mp_ignore_d("qweas0zxc");
        inline constexpr mapperT mp_ignore_z("qweasd0xc");
        inline constexpr mapperT mp_ignore_x("qweasdz0c");
        inline constexpr mapperT mp_ignore_c("qweasdzx0");

        // TODO: clarify the [exact] meaning (&&effects) of these mappers...
        // Native symmetry.
        inline constexpr mapperT mp_refl_asd("zxc"
                                             "asd"
                                             "qwe"); // '-'
        inline constexpr mapperT mp_refl_wsx("ewq"
                                             "dsa"
                                             "cxz"); // '|'
        inline constexpr mapperT mp_refl_qsc("qaz"
                                             "wsx"
                                             "edc"); // '\'
        inline constexpr mapperT mp_refl_esz("cde"
                                             "xsw"
                                             "zaq"); // '/'
        inline constexpr mapperT mp_C2("cxz"
                                       "dsa"
                                       "ewq"); // 180
        inline constexpr mapperT mp_C4("zaq"
                                       "xsw"
                                       "cde"); // 90 (clockwise)
        // 90 has the same effects as 270... TODO: recheck...

        // TODO: explain; actually irrelevant of symmetry...
        // 1. I misunderstood "rotate" symmetry. "ro45" is never about symmetry (I've no idea what it is)
        // 2. As seemingly-senseless partition like ro45 can make non-trivial patterns, should support after all...
        // TODO: explain C8 (not related to static symmetry, but covers C4 and works with tot_xxc_s)
        inline constexpr mapperT mp_C8("aqw"
                                       "zse"
                                       "xcd");
        inline constexpr mapperT mp_tot_exc_s("wqe"
                                              "asd"
                                              "zxc"); // swap(q,w); *C8 -> totalistic, excluding s
        inline constexpr mapperT mp_tot_inc_s("qse"
                                              "awd"
                                              "zxc"); // swap(w,s); *C8 -> totalistic, including s

        // TODO: explain. TODO: better name...
        inline constexpr mapperT mp_dual("!q!w!e"
                                         "!a!s!d"
                                         "!z!x!c");

        // Hexagonal emulation and emulated symmetry.

        // q w -     q w
        // a s d -> a s d
        // - x c     x c
        inline constexpr mapperT mp_hex_ignore("qw0"
                                               "asd"
                                               "0xc"); // ignore_(e,z)

        inline constexpr mapperT mp_hex_refl_asd("xc0"
                                                 "asd"
                                                 "0qw"); // swap(q,x), swap(w,c)
        inline constexpr mapperT mp_hex_refl_qsc("qa0"
                                                 "wsx"
                                                 "0dc"); // swap(a,w), swap(x,d)
        inline constexpr mapperT mp_hex_refl_wsx("dw0"
                                                 "csq"
                                                 "0xa"); // swap(q,d), swap(a,c)

        inline constexpr mapperT mp_hex_refl_aq("ax0"
                                                "qsc"
                                                "0wd"); // swap(a,q), swap(x,w), swap(c,d)
        inline constexpr mapperT mp_hex_refl_qw("wq0"
                                                "dsa"
                                                "0cx"); // swap(q,w), swap(a,d), swap(x,c)
        inline constexpr mapperT mp_hex_refl_wd("cd0"
                                                "xsw"
                                                "0aq"); // swap(w,d), swap(q,c), swap(a,x)

        inline constexpr mapperT mp_hex_C2("cx0"
                                           "dsa"
                                           "0wq"); // 180
        inline constexpr mapperT mp_hex_C3("xa0"
                                           "csq"
                                           "0dw"); // 120 (clockwise)
        inline constexpr mapperT mp_hex_C6("aq0"
                                           "xsw"
                                           "0cd"); // 60 (clockwise)

        inline constexpr mapperT mp_hex_tot_exc_s("wq0"
                                                  "asd"
                                                  "0xc"); // swap(q,w); *C6 -> totalistic, excluding s
        inline constexpr mapperT mp_hex_tot_inc_s("qs0"
                                                  "awd"
                                                  "0xc"); // swap(w,s); *C6 -> totalistic, including s

#if 0
        // It's also valid to emulate hexagonal neighborhood by ignoring "q" and "c".
        // However, the program is not going to support this, as it makes the program more complicated
        // but without much benefit.

        // - w e     w e
        // a s d -> a s d
        // z x -     z x
        inline constexpr mapperT mp_hex2_ignore("0we"
                                                "asd"
                                                "zx0"); // ignore_(q,c)
#endif

        inline subsetT make_subset(std::initializer_list<mapperT> mappers, const maskT& mask = mask_zero) {
            equivT eq{};
            for (const mapperT& m : mappers) {
                add_eq(eq, {mp_identity, m});
            }
            return subsetT{mask, eq};
        }

    } // namespace _make_subset

#ifdef ENABLE_TESTS
    namespace _tests {
        inline const testT test_mappers = [] {
            for_each_code([](codeT code) {
                assert(mp_identity(code) == code);

                for (const mapperT* m : {&mp_refl_asd, &mp_refl_wsx, &mp_refl_qsc, &mp_refl_esz, &mp_C2}) {
                    assert((*m)((*m)(code)) == code);
                }
                assert(mp_C4(mp_C4(mp_C4(mp_C4(code)))) == code);

                assert(mp_dual(mp_dual(code)) == code);

                // ... TODO...
            });
        };

        inline const testT test_subset_intersection = [] {
            subsetT sc = make_subset({mp_ignore_s}, mask_zero) & make_subset({mp_dual}, mask_identity);

            // 2024/1/20 2AM
            // There is NO problem in the algorithm.
            // It's just that, in this situation the maskT has a strong bias, so that it's too easy to generate
            // rules in a certain direction...
            using enum codeT::bposE;
            assert(!sc.empty());
            assert(sc.contains(make_mask(bpos_q)));
            assert(sc.contains(make_mask(bpos_w)));
            assert(sc.contains(make_mask(bpos_e)));
            assert(sc.contains(make_mask(bpos_a)));
            assert(!sc.contains(mask_identity)); // Doesn't contain make_mask(bpos_s).
            assert(sc.contains(make_mask(bpos_d)));
            assert(sc.contains(make_mask(bpos_z)));
            assert(sc.contains(make_mask(bpos_x)));
            assert(sc.contains(make_mask(bpos_c)));
        };
    }  // namespace _tests
#endif // ENABLE_TESTS

} //  namespace legacy
