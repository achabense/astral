#include <deque>

#include "rule_algo.hpp"

#include "common.hpp"

// TODO: support navigation among enter-buttons?
static bool enter_button(const char* label) {
    static ImGuiID bind_id = 0;
    bool ret = ImGui::Button(label);
    const ImGuiID button_id = ImGui::GetItemID();
    if (ret) {
        bind_id = button_id;
    }
    // TODO: instead of relying on Begin/EndDisabled, add click-protection
    // on a button-by-button basis?
    // TODO: are there public ways (not relying on imgui_internal.h)
    // to detect whether in disabled block?
    if (bind_id == button_id && (GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) == 0) {
        if (imgui_KeyPressed(ImGuiKey_Enter, false)) {
            ret = true;
        }
        imgui_ItemRect(ret ? IM_COL32(128, 128, 128, 255) : IM_COL32_WHITE);
    }
    return ret;
}

class subset_selector {
    legacy::subsetT current;

    struct termT {
        const char* const title;
        const legacy::subsetT set;
        const char* const description = nullptr;
        bool selected = false;
        // bool includes_cur = false; // TODO: whether to cache this?
        bool disabled = false; // current & set -> empty.

        bool includes(const termT& other) const { return set.includes(other.set); }
    };

    using termT_list = std::deque<termT>;

    termT_list terms_ignore; // TODO: rename...
    termT_list terms_misc;
    termT_list terms_native;
    termT_list terms_totalistic;
    termT_list terms_hex;
    // TODO: about the plan to support user-defined subsets...

    void for_each_term(const auto& fn) {
        for (termT_list* terms : {&terms_ignore, &terms_misc, &terms_native, &terms_totalistic, &terms_hex}) {
            for (termT& t : *terms) {
                fn(t);
            }
        }
    }

    void update_current() {
        current = legacy::subsetT::universal();

        for_each_term([&](termT& t) {
            assert(!t.disabled || !t.selected); // disabled -> !selected
            if (t.selected) {
                current = current & t.set;
            }
        });

        assert(!current.empty());

        for_each_term([&](termT& t) { //
            t.disabled = !legacy::subsetT::common_rule(t.set, current);
        });
    }

public:
    subset_selector() : current(legacy::subsetT::universal()) {
        using namespace legacy::_make_subset;
        // TODO: add descriptions...

        const termT& ignore_q = terms_ignore.emplace_back("q", make_subset({mp_ignore_q}));
        terms_ignore.emplace_back("w", make_subset({mp_ignore_w}));
        const termT& ignore_e = terms_ignore.emplace_back("e", make_subset({mp_ignore_e}));
        terms_ignore.emplace_back("a", make_subset({mp_ignore_a}));
        terms_ignore.emplace_back("s", make_subset({mp_ignore_s}, mask_zero),
                                  "0->1, 1->1 or 0->0, 1->0"); // TODO: whether to put in terms_ignore?
        terms_ignore.emplace_back("d", make_subset({mp_ignore_d}));
        const termT& ignore_z = terms_ignore.emplace_back("z", make_subset({mp_ignore_z}));
        terms_ignore.emplace_back("x", make_subset({mp_ignore_x}));
        const termT& ignore_c = terms_ignore.emplace_back("c", make_subset({mp_ignore_c}));

        terms_misc.emplace_back("S(f)", make_subset({mp_ignore_s}, mask_identity), "0->0, 1->1 or 0->1, 1->0");
        const termT& ignore_hex = terms_misc.emplace_back("Hex", make_subset({mp_hex_ignore}));
        const termT& ignore_von = terms_misc.emplace_back("Von", make_subset({mp_von_ignore}));
        terms_misc.emplace_back("Dual", make_subset({mp_dual}, mask_identity)); // Self-complementary
        // terms_misc.emplace_back("'C8'", make_subset({mp_C8}));

        const termT& native_isotropic = terms_native.emplace_back("All", make_subset({mp_refl_wsx, mp_refl_qsc}));
        terms_native.emplace_back("|", make_subset({mp_refl_wsx}));
        terms_native.emplace_back("-", make_subset({mp_refl_asd}));
        terms_native.emplace_back("\\", make_subset({mp_refl_qsc}));
        terms_native.emplace_back("/", make_subset({mp_refl_esz}));
        const termT& native_C2 = terms_native.emplace_back("C2", make_subset({mp_C2}));
        const termT& native_C4 = terms_native.emplace_back("C4", make_subset({mp_C4}));

        const termT& native_tot_exc_s = terms_totalistic.emplace_back("Tot", make_subset({mp_C8, mp_tot_exc_s}));
        const termT& native_tot_inc_s = terms_totalistic.emplace_back("Tot(+s)", make_subset({mp_C8, mp_tot_inc_s}));
        const termT& hex_tot_exc_s =
            terms_totalistic.emplace_back("Hex_Tot", make_subset({mp_hex_C6, mp_hex_tot_exc_s}));
        const termT& hex_tot_inc_s =
            terms_totalistic.emplace_back("Hex_Tot(+s)", make_subset({mp_hex_C6, mp_hex_tot_inc_s}));

        const termT& hex_isotropic = terms_hex.emplace_back("All", make_subset({mp_hex_refl_asd, mp_hex_refl_aq}));
        terms_hex.emplace_back("a-d", make_subset({mp_hex_refl_asd}));
        terms_hex.emplace_back("q-c", make_subset({mp_hex_refl_qsc}));
        terms_hex.emplace_back("w-x", make_subset({mp_hex_refl_wsx}));
        terms_hex.emplace_back("a|q", make_subset({mp_hex_refl_aq}));
        terms_hex.emplace_back("q|w", make_subset({mp_hex_refl_qw}));
        terms_hex.emplace_back("w|d", make_subset({mp_hex_refl_wd}));
        const termT& hex_C2 = terms_hex.emplace_back("C2", make_subset({mp_hex_C2}));
        const termT& hex_C3 = terms_hex.emplace_back("C3", make_subset({mp_hex_C3}));
        const termT& hex_C6 = terms_hex.emplace_back("C6", make_subset({mp_hex_C6}));

        update_current();

        // [[maybe_unused]] will make lines too long...
        (void)ignore_q, (void)ignore_e, (void)ignore_z, (void)ignore_c;
        (void)ignore_hex, (void)ignore_von;
        (void)native_isotropic, (void)native_C2, (void)native_C4;
        (void)native_tot_exc_s, (void)native_tot_inc_s, (void)hex_tot_exc_s, (void)hex_tot_inc_s;
        (void)hex_isotropic, (void)hex_C2, (void)hex_C3, (void)hex_C6;

        assert(ignore_e.includes(ignore_hex));
        assert(ignore_z.includes(ignore_hex));
        assert(ignore_q.includes(ignore_von));
        assert(ignore_e.includes(ignore_von));
        assert(ignore_z.includes(ignore_von));
        assert(ignore_c.includes(ignore_von));

        assert(native_C2.set.includes(native_C4.set));
        assert(std::ranges::all_of(terms_native, [&](const termT& t) { return t.includes(native_isotropic); }));
        assert(native_isotropic.includes(native_tot_exc_s));
        assert(native_tot_exc_s.includes(native_tot_inc_s));

        assert(std::ranges::all_of(terms_hex, [&](const termT& t) { return ignore_hex.includes(t); }));
        assert(hex_C2.includes(hex_C6));
        assert(hex_C3.includes(hex_C6));
        assert(std::ranges::all_of(terms_hex, [&](const termT& t) { return t.includes(hex_isotropic); }));
        assert(hex_isotropic.includes(hex_tot_exc_s));
        assert(hex_tot_exc_s.includes(hex_tot_inc_s));
    }

    // TODO: `mold` is the analysis target, rename and explain.
    const legacy::subsetT& select_subset(const legacy::moldT& mold) {
        {
            // https://github.com/ocornut/imgui/issues/6902
            const float extra_w_sameline = ImGui::GetStyle().ItemSpacing.x * 2; // Two SameLine...
            const float extra_w_padding = ImGui::GetStyle().FramePadding.x * 4; // Two Button * two sides...
            const float extra_w = ImGui::CalcTextSize("ClearRecognize").x + extra_w_sameline + extra_w_padding;
            ImGui::SeparatorTextEx(0, "Select subsets", nullptr, extra_w);
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                for_each_term([&](termT& t) { t.disabled = t.selected = false; });
                update_current();
            }
            ImGui::SameLine();
            if (ImGui::Button("Recognize")) {
                for_each_term([&](termT& t) {
                    t.disabled = false; // Will be updated by `update_current`.
                    t.selected = t.set.contains(mold.rule);
                });
                update_current();
            }
        }

        // TODO: drop mutable id... use manually specified ids
        // TODO: tooltip...
        auto check = [&, id = 0, size = square_size()](termT& term) mutable {
            ImGui::PushID(id++);
            // ImGui::PushID(&term); // TODO: is ptr id reliable? (will this possibly cause collisions?)
            if (ImGui::InvisibleButton("Check", size) && !term.disabled) {
                term.selected = !term.selected;
                update_current();
            }
#if 0 // Convenient but not necessary...
            else if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Right) && term.selected) {
                assert(!term.disabled);
                term.selected = false;
                update_current();
            }
#endif
            ImGui::PopID();

            // TODO: use helper::show_help instead.
            if (ImGui::BeginItemTooltip()) {
                imgui_Str(term.description ? term.description : "TODO");
                ImGui::EndTooltip();
            }

            // TODO: refine...
            // TODO: explain coloring scheme; redesign if necessary (especially ring col)
            // TODO: find better color for "disabled"/incompatible etc... currently too ugly.
            const ImU32 cen_col = term.selected                ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
                                  : term.set.includes(current) ? ImGui::GetColorU32(ImGuiCol_FrameBg)
                                  : term.disabled              ? IM_COL32(120, 30, 0, 255)
                                                               : IM_COL32_BLACK;
            const ImU32 ring_col = term.set.contains(mold.rule) ? IM_COL32(0, 255, 0, 255)
                                   : compatible(term.set, mold) ? IM_COL32(0, 100, 0, 255)
                                                                : IM_COL32(200, 45, 0, 255);

            imgui_ItemRectFilled(IM_COL32_BLACK);
            imgui_ItemRectFilled(cen_col, term.disabled ? ImVec2(5, 5) : ImVec2(4, 4));
            imgui_ItemRect(ring_col);
            if (!term.disabled && ImGui::IsItemHovered()) {
                imgui_ItemRectFilled(IM_COL32(255, 255, 255, 45));
            }
        };

        if (ImGui::BeginTable("Checklists", 2, ImGuiTableFlags_BordersInner | ImGuiTableFlags_SizingFixedFit)) {
            auto put_row = [](const char* l_str, const auto& r_logic) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                imgui_Str(l_str);

                ImGui::TableNextColumn();
                r_logic();
            };

            auto checklist = [&](termT_list& terms) {
                for (bool first = true; termT & t : terms) {
                    if (!std::exchange(first, false)) {
                        ImGui::SameLine();
                    }
                    ImGui::BeginGroup();
                    imgui_Str(t.title);
                    check(t);
                    ImGui::EndGroup();
                }
            };

            put_row("Ignore & Misc", [&] {
                // TODO: slightly confusing; light color should represent "take-into-account" instead of
                // "ignore" Is this solvable by applying specific coloring scheme?
                ImGui::BeginGroup();
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));
                for (int l = 0; l < 3; ++l) {
                    check(terms_ignore[l * 3 + 0]);
                    ImGui::SameLine();
                    check(terms_ignore[l * 3 + 1]);
                    ImGui::SameLine();
                    check(terms_ignore[l * 3 + 2]);
                }
                ImGui::PopStyleVar();
                ImGui::EndGroup();

                ImGui::SameLine();
                checklist(terms_misc);
            });

            put_row("Native", [&] { checklist(terms_native); });
            put_row("Totalistic", [&] { checklist(terms_totalistic); });
            put_row("q w -    q w\n"
                    "a s d ~ a s d\n"
                    "- x c    x c",
                    [&] { checklist(terms_hex); });

            ImGui::EndTable();
        }

        assert(!current.empty());
        return current;
    }
};

std::optional<legacy::moldT> edit_rule(const legacy::moldT& mold, const code_icons& icons) {
    std::optional<legacy::moldT> out = std::nullopt;
    auto return_rule = [&out, &mold](const legacy::ruleT& rule) { out.emplace(rule, mold.lock); };
    auto return_lock = [&out, &mold](const legacy::moldT::lockT& lock) { out.emplace(mold.rule, lock); };
    auto return_mold = [&out](const legacy::moldT& mold) { out.emplace(mold); };

    // pass_* are data passed from the previous frame.
    static std::optional<legacy::moldT> pass_analysis_target = std::nullopt;
    static subset_selector selector;
    const legacy::subsetT& subset = selector.select_subset(pass_analysis_target.value_or(mold));
    assert(!subset.empty());
    const legacy::partitionT& par = subset.get_par();

    if (pass_analysis_target) {
        pass_analysis_target.reset();
    }

    ImGui::Separator();

    // Select mask.
    static std::optional<legacy::ruleT> pass_custom_mask = std::nullopt;
    char chr_0 = '0', chr_1 = '1';
    const legacy::maskT& mask = [&] {
        // TODO: finish...
        const char* const about_mask = "About mask:\n"
                                       "A mask is an arbitrary rule used to do XOR masking for other rules...\n"
                                       "When the rule doesn't actually belong to the selected subsets ...";

        // TODO: support rollbacking custom masks?
        static legacy::maskT mask_custom{{}};

        // TODO: finish descriptions (use cases etc)
        static const char* const mask_labels[]{"Zero", "Identity", "Native", "Custom"};
        static const char* const mask_descriptions[]{
            "The all-zero rule.\n"
            "Masked by this you see the actual values, and ...",

            "The rule that maps each situation to the center cell itself, so any pattern will keep unchanged under "
            "this rule. (Click \"<00..\" button to set to it for test.)\n"
            "As the masking rule it shows how \"volatile\" a rule is ...", // TODO: "<00.." is not a good label...

            "A specific rule known to belong to the selected subsets, so that it is guaranteed to be able to support "
            "editions...", // (may or may not be the all-zero rule/identity rule depending on the subsets you have
                           // selected...)

            "Custom rule; you can click \"Take current\" button to set this to the current rule.\n"
            "Important tip: ..."};
        static int mask_tag = 0;

        if (pass_custom_mask) {
            mask_tag = 3;
            mask_custom = {*pass_custom_mask};
            pass_custom_mask.reset();
        }

        // TODO: the support for other make_mask(bpos_* (other than bpos_s)) was poorly designed and dropped.
        // Redesign to add back these masks.
        const legacy::maskT* const mask_ptrs[]{&legacy::mask_zero, &legacy::mask_identity, &subset.get_mask(),
                                               &mask_custom};

        ImGui::AlignTextToFramePadding();
        imgui_Str("Mask");
        helper::show_help(about_mask);

        for (int i = 0; i < 4; ++i) {
            ImGui::SameLine(0, imgui_ItemInnerSpacingX());

            if (ImGui::RadioButton(mask_labels[i], mask_tag == i)) {
                mask_tag = i;
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                pass_analysis_target.emplace(*mask_ptrs[i]);
            }

            helper::show_help([&] {
                // TODO: will unpaired push like this cause leakage? (is the style-stack regularly cleared, or will
                // this accumulate?)
                // ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0);
                imgui_Str(mask_descriptions[i]);
                imgui_Str(legacy::to_MAP_str(*mask_ptrs[i]));
            });
        }

        ImGui::SameLine();
        if (ImGui::Button("Take current")) {
            mask_tag = 3;
            mask_custom = {mold.rule};
        }

        // TODO: horrible...
        switch (mask_tag) {
            case 0: chr_0 = '0', chr_1 = '1'; break;
            case 1: chr_0 = '.', chr_1 = '!'; break;
            default: chr_0 = 'o', chr_1 = 'i'; break;
        }

        return *mask_ptrs[mask_tag];
    }();

    ImGui::Separator();

    // Disable all edit operations if !subset.contains(mask), including those that do not really need
    // the mask to be valid (or even do not depend on subsetT).
    // (These operations include: ... TODO: list all)
    // This makes sense as:
    // 1. Such operations are not many, and they are typically used in combination with those that
    // do need correct masks.
    // 2. The program already provides a way to get always-usable mask (the "native" mode).
    if (!subset.contains(mask)) {
        // TODO: complete message. other suggestions.
        imgui_StrWrapped(
            "This mask (?rule?) doesn't belong to the selected subsets. Try other masks ... the \"Native\" mask will "
            "always work... Right-click the masks to see what subsets they apply to.");
        // TODO: the native and custom mode has mutable mask, will the temp-analysis be misleading for them?
        return std::nullopt;
    }

    const bool compatible = legacy::compatible(subset, mold);
    const bool contained = subset.contains(mold.rule);
    assert(!contained || compatible); // contained -> compatible

    const auto scanlist = legacy::scan(par, mask, mold);

    {
        const auto [c_free, c_locked_0, c_locked_1] = [&] {
            const int c_group = par.k();
            int c_0 = 0, c_1 = 0, c_x = 0;
            int c_free = 0;
            int c_locked_0 = 0, c_locked_1 = 0, c_locked_x = 0;
            for (const auto& scan : scanlist) {
                if (scan.all_0()) {
                    ++c_0;
                } else if (scan.all_1()) {
                    ++c_1;
                } else {
                    // c_x: number of groups that make `mold.rule` uncontained.
                    ++c_x;
                }

                if (!scan.locked_0 && !scan.locked_1) {
                    ++c_free;
                } else if (!scan.locked_0 && scan.locked_1) {
                    ++c_locked_1;
                } else if (scan.locked_0 && !scan.locked_1) {
                    ++c_locked_0;
                } else {
                    // c_locked_x: number of groups that make `mold` incompatible.
                    ++c_locked_x;
                }
            }

            assert(contained == !c_x);
            assert(compatible == !c_locked_x);

            // TODO: refine...
            std::string summary = std::format("Groups:{} ({}:{} {}:{} x:{})", c_group, chr_0, c_0, chr_1, c_1, c_x);
            if (c_free != c_group) {
                summary += std::format(" [Locked:{} ({}:{} {}:{} x:{})]", c_group - c_free, chr_0, c_locked_0, chr_1,
                                       c_locked_1, c_locked_x);
            }
            imgui_Str(summary);

            assert(c_free + c_locked_0 + c_locked_1 + c_locked_x == c_group);
            return std::array{c_free, c_locked_0, c_locked_1};
        }();

        if (!compatible) {
            ImGui::BeginDisabled();
        }

        // dist: The "distance" to the masking rule the randomization want to achieve.
        // (which does not make sense when !compatible)
        static double rate = 0.5;
        int dist = c_locked_1 + round(rate * c_free);

        static bool exact_mode = true;
        if (ImGui::Button(exact_mode ? "Exactly###Mode" : "Around ###Mode")) {
            exact_mode = !exact_mode;
        }

        ImGui::SameLine(0, imgui_ItemInnerSpacingX());
        ImGui::SetNextItemWidth(item_width);
        if (imgui_StepSliderInt("##Quantity", &dist, c_locked_1, c_locked_1 + c_free) && c_free != 0) {
            rate = double(dist - c_locked_1) / c_free;
            assert(c_locked_1 + round(rate * c_free) == dist);
        }
        ImGui::SameLine(0, imgui_ItemInnerSpacingX());
        if (enter_button("Randomize")) {
            if (exact_mode) {
                return_rule(legacy::randomize_c(subset, mask, mold, global_mt19937(), dist - c_locked_1));
            } else {
                return_rule(legacy::randomize_p(subset, mask, mold, global_mt19937(), rate));
            }
        }
    }

    // TODO: it looks strange when only middle part is disabled...
    iter_group(
        "<00..", "dec", "inc", "11..>", //
        [&] { return_rule(legacy::seq_int::min(subset, mask, mold)); },
        [&] { return_rule(legacy::seq_int::dec(subset, mask, mold)); },
        [&] { return_rule(legacy::seq_int::inc(subset, mask, mold)); },
        [&] { return_rule(legacy::seq_int::max(subset, mask, mold)); }, contained ? enter_button : nullptr);
    ImGui::SameLine(), imgui_Str("|"), ImGui::SameLine();

    if (!contained) {
        ImGui::BeginDisabled();
    }
    iter_group(
        "<1.0.", "prev", "next", "0.1.>", //
        [&] { return_rule(legacy::seq_perm::first(subset, mask, mold)); },
        [&] { return_rule(legacy::seq_perm::prev(subset, mask, mold)); },
        [&] { return_rule(legacy::seq_perm::next(subset, mask, mold)); },
        [&] { return_rule(legacy::seq_perm::last(subset, mask, mold)); }, enter_button);

    ImGui::SameLine(), imgui_Str("|"), ImGui::SameLine();
    // TODO: better name for "dial"?
    // TODO: when to use Abc vs abc?
    if (ImGui::Button("dial")) {
        pass_custom_mask.emplace(mold.rule);
        return_rule(legacy::seq_int::one(subset, {mold.rule}, mold));
    }
    helper::show_help("Equivalent to..."); // TODO...

    // TODO: enhance might be stricter than necessary...
    if (ImGui::Button("Enhance lock")) {
        return_lock(legacy::enhance_lock(subset, mold));
    }
    if (!contained) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (ImGui::Button("Approximate")) {
        return_rule(legacy::approximate(subset, mold));
    }
    if (!compatible) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear lock")) {
        return_lock({});
    }
    ImGui::SameLine(), imgui_Str("|"), ImGui::SameLine();
    // TODO: move elsewhere
    if (ImGui::Button("Mir")) {
        return_mold(legacy::trans_mirror(mold));
    }

    // TODO: support filtering?
    if (auto child = imgui_ChildWindow("Details")) {
        const char labels[2][3]{{'-', chr_0, '\0'}, {'-', chr_1, '\0'}};
        const legacy::ruleT_masked masked = mask ^ mold.rule;

        // Precise vertical alignment:
        // https://github.com/ocornut/imgui/issues/2064
        const auto align_text = [](float height) {
            const float off = std::max(0.0f, -1.0f + (height - ImGui::GetTextLineHeight()) / 2);
            ImGui::SetCursorPosY(floor(ImGui::GetCursorPos().y + off));
        };

        const int zoom = 7;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        par.for_each_group([&](int j, const legacy::groupT& group) {
            if (j % 8 != 0) {
                ImGui::SameLine(0, 12);
            }
            if (j != 0 && j % 64 == 0) {
                ImGui::Separator();
            }
            const bool inconsistent = scanlist[j].inconsistent();
            const legacy::codeT head = group[0];
            const bool has_lock = scanlist[j].any_locked();

            if (inconsistent) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0, 0, 1));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0, 0, 1));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0, 0, 1));
            }
            if (icons.button(head, zoom)) {
                legacy::ruleT rule = mold.rule;
                for (legacy::codeT code : group) {
                    rule[code] = !rule[code];
                }
                return_rule(rule);
            } else if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                legacy::moldT::lockT lock = mold.lock;
                for (legacy::codeT code : group) {
                    lock[code] = !has_lock;
                }
                return_lock(lock);
            }
            if (inconsistent) {
                ImGui::PopStyleColor(3);
            }

            if (ImGui::BeginItemTooltip()) {
                ImGui::Text("Group size: %d", (int)group.size());
                const int max_to_show = 48;
                for (int x = 0; auto code : group) {
                    if (x++ % 8 != 0) {
                        ImGui::SameLine();
                    }
                    // TODO: change color?
                    // ImGui::GetStyle().Colors[ImGuiCol_Button]
                    icons.image(code, zoom, ImVec4(1, 1, 1, 1), ImVec4(0.5, 0.5, 0.5, 1));
                    ImGui::SameLine(0, imgui_ItemInnerSpacingX());
                    align_text(ImGui::GetItemRectSize().y);
                    imgui_Str(labels[masked[code]]);
                    if (mold.lock[code]) {
                        imgui_ItemRect(IM_COL32_WHITE, ImVec2(-2, -2));
                    }
                    if (x == max_to_show) {
                        break;
                    }
                }
                if (group.size() > max_to_show) {
                    imgui_Str("...");
                }
                ImGui::EndTooltip();
            }

            const float button_height = ImGui::GetItemRectSize().y;
            ImGui::SameLine(0, imgui_ItemInnerSpacingX());
            align_text(button_height);
            imgui_Str(labels[masked[head]]);

            if (has_lock) {
                const ImU32 col = scanlist[j].all_locked() ? IM_COL32_WHITE : IM_COL32(128, 128, 128, 255);
                imgui_ItemRect(col, ImVec2(-2, -2));
            }
        });
        ImGui::PopStyleVar(1);
    }

    return out;
}

std::optional<legacy::moldT> static_constraints() {
    enum stateE { Any_background, O, I, O_background, I_background };
    const int r = 9;
    static stateE board[r][r]{/*Any_background...*/};
    static stateE state_lbutton = I;
    const stateE state_rbutton = Any_background;
    static const ImU32 cols[5]{IM_COL32(100, 100, 100, 255), //
                               IM_COL32(0, 0, 0, 255),       //
                               IM_COL32(255, 255, 255, 255), //
                               IM_COL32(80, 0, 80, 255),     //
                               IM_COL32(200, 0, 200, 255)};

    const bool ret = ImGui::Button("Done");
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        for (auto& l : board) {
            for (auto& s : l) {
                s = Any_background;
            }
        }
    }

    // Display-only; the value of `state_lbutton` is controlled by mouse-scrolling.
    ImGui::BeginDisabled();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {2, 2});
    for (const stateE s : {O, I, O_background, I_background}) {
        if (s != O) {
            ImGui::SameLine(0, imgui_ItemInnerSpacingX());
        }
        // No need for unique ID here (as the return value is not used).
        ImGui::RadioButton("##Radio", s == state_lbutton);
        ImGui::SameLine(0, imgui_ItemInnerSpacingX());
        ImGui::Dummy(square_size());
        imgui_ItemRectFilled(cols[s]);
        imgui_ItemRect(IM_COL32(200, 200, 200, 255));
    }
    ImGui::PopStyleVar();
    ImGui::EndDisabled();

    ImGui::BeginGroup();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, 0});
    for (int y = 0; y < r; ++y) {
        for (int x = 0; x < r; ++x) {
            if (x != 0) {
                ImGui::SameLine();
            }

            // No need for unique ID here (as IsItemHovered + IsMouseDown doesn't reply on ID).
            ImGui::InvisibleButton("##Invisible", square_size());

            const bool editable = y >= 1 && y < r - 1 && x >= 1 && x < r - 1;
            stateE& state = board[y][x];
            imgui_ItemRectFilled(!editable ? IM_COL32(80, 80, 80, 255) : cols[state]);
            imgui_ItemRect(IM_COL32(200, 200, 200, 255));

            if (editable && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
                if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    state = state_rbutton;
                } else if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    state = state_lbutton;
                }
            }
        }
    }
    ImGui::PopStyleVar();
    ImGui::EndGroup();
    if (ImGui::IsItemHovered()) {
        if (imgui_MouseScrollingDown()) {
            state_lbutton = (stateE)std::min((int)I_background, state_lbutton + 1);
        } else if (imgui_MouseScrollingUp()) {
            state_lbutton = (stateE)std::max((int)O, state_lbutton - 1);
        }
    }

    if (ret) {
        legacy::moldT mold{};
        for (int y = 1; y < r - 1; ++y) {
            for (int x = 1; x < r - 1; ++x) {
                if (board[y][x] == O || board[y][x] == I) {
                    // For example:
                    //  O   O_b  I                  001       001
                    // [Any] O   O  will result in [0]00 and [1]00 being set.
                    //  I_b  I  I_b                 111       111
                    legacy::for_each_code([&](legacy::codeT code) {
                        auto imbue = [](bool& b, stateE state) {
                            if (state == O || state == O_background) {
                                b = 0;
                            } else if (state == I || state == I_background) {
                                b = 1;
                            }
                        };

                        legacy::situT situ = legacy::decode(code);

                        imbue(situ.q, board[y - 1][x - 1]);
                        imbue(situ.w, board[y - 1][x]);
                        imbue(situ.e, board[y - 1][x + 1]);
                        imbue(situ.a, board[y][x - 1]);
                        imbue(situ.s, board[y][x]);
                        imbue(situ.d, board[y][x + 1]);
                        imbue(situ.z, board[y + 1][x - 1]);
                        imbue(situ.x, board[y + 1][x]);
                        imbue(situ.c, board[y + 1][x + 1]);

                        mold.rule[legacy::encode(situ)] = board[y][x] == O ? 0 : 1;
                        mold.lock[legacy::encode(situ)] = true;
                    });
                }
            }
        }
        return mold;
    }
    return std::nullopt;
}
