#define IMGUI_DEFINE_MATH_OPERATORS

#include "app2.hpp"
#include "app_sdl.hpp"
#include "imgui_internal.h" // TODO: for `RenderNavHighlight`

#include "rule_pp.hpp"

namespace legacy {
    // TODO: move elsewhere
    // TODO: proper name...
    inline ruleT mirror(const ruleT& rule) {
        ruleT mir{};
        for_each_code(code) {
            codeT codex = flip_all(code);
            const bool flip = decode_s(codex) != rule(codex);
            mir.set(code, flip ? !decode_s(code) : decode_s(code));
        }
        return mir;
    }

    struct termT {
        const char* msg;
        equivT eq;

        bool selected = false;
        bool covered = false;
    };
    // TODO: refine analyzer...
    class partition_collection {
        using termT_vec = std::vector<termT>;

        termT_vec terms_ignore;
        termT_vec terms_native;
        termT_vec terms_misc;
        termT_vec terms_hex;

        // TODO: customized...

        std::optional<partitionT> par;

        void reset_par() {
            equivT q{};
            auto set = [&q](termT_vec& terms) {
                for (termT& t : terms) {
                    if (t.selected) {
                        q.add_eq(t.eq);
                    }
                }
            };
            set(terms_ignore);
            set(terms_native);
            set(terms_misc);
            set(terms_hex);

            auto test = [&q](termT_vec& terms) {
                for (termT& t : terms) {
                    t.covered = q.has_eq(t.eq);
                }
            };

            test(terms_ignore);
            test(terms_native);
            test(terms_misc);
            test(terms_hex);

            par.emplace(q);
        }

    public:
        partition_collection() {
            auto mk = [](std::initializer_list<mapperT> ms) {
                equivT eq{};
                for (const mapperT& m : ms) {
                    eq.add_eq({mp_identity, m}); // TODO: expose mp_identity?
                }
                return eq;
            };

            terms_ignore.emplace_back("q", mk({mp_ignore_q}));
            terms_ignore.emplace_back("w", mk({mp_ignore_w}));
            terms_ignore.emplace_back("e", mk({mp_ignore_e}));
            terms_ignore.emplace_back("a", mk({mp_ignore_a}));
            terms_ignore.emplace_back("s", mk({mp_ignore_s}));
            terms_ignore.emplace_back("d", mk({mp_ignore_d}));
            terms_ignore.emplace_back("z", mk({mp_ignore_z}));
            terms_ignore.emplace_back("x", mk({mp_ignore_x}));
            terms_ignore.emplace_back("c", mk({mp_ignore_c}));

            terms_native.emplace_back("|", mk({mp_refl_wsx}));
            terms_native.emplace_back("-", mk({mp_refl_asd}));
            terms_native.emplace_back("\\", mk({mp_refl_qsc}));
            terms_native.emplace_back("/", mk({mp_refl_esz}));
            terms_native.emplace_back("C2(180)", mk({mp_C2}));
            terms_native.emplace_back("C4(90)", mk({mp_C4}));

            terms_misc.emplace_back("*R45", mk({mp_ro_45}));
            terms_misc.emplace_back("*Tota", mk({mp_ro_45, mp_tot_a}));
            terms_misc.emplace_back("*Totb", mk({mp_ro_45, mp_tot_b}));
            terms_misc.emplace_back("Dual", mk({mp_dual}));

            terms_hex.emplace_back("Hex", mk({mp_hex_ignore}));
            terms_hex.emplace_back("/", mk({mp_hex_refl_wsx}));
            terms_hex.emplace_back("\\", mk({mp_hex_refl_qsc}));
            terms_hex.emplace_back("-", mk({mp_hex_refl_asd}));
            terms_hex.emplace_back("aq", mk({mp_hex_refl_aq}));
            terms_hex.emplace_back("qw", mk({mp_hex_refl_qw}));
            terms_hex.emplace_back("wd", mk({mp_hex_refl_wd}));

            terms_hex.emplace_back("C2(180)", mk({mp_hex_C2}));
            terms_hex.emplace_back("C3(120)", mk({mp_hex_C3}));
            terms_hex.emplace_back("C6(60)", mk({mp_hex_C6}));

            terms_hex.emplace_back("*Tota", mk({mp_hex_C6, mp_hex_tot_a}));
            terms_hex.emplace_back("*Totb", mk({mp_hex_C6, mp_hex_tot_b}));

            reset_par();
        }

        const partitionT& get_par(const ruleT& target) {
            // TODO: tooltip...
            /*
            const auto show_pair = [](const mapperT_pair& q) {
                std::string up, cn, dw;
                // TODO: too clumsy...
                static const char* const strs[]{" 0", " 1", " q", " w", " e", " a", " s", " d", " z", " x",
                                                " c", "!q", "!w", "!e", "!a", "!s", "!d", "!z", "!x", "!c"};

                up += strs[q.a.q2];
                up += strs[q.a.w2];
                up += strs[q.a.e2];
                up += "  ";
                up += strs[q.b.q2];
                up += strs[q.b.w2];
                up += strs[q.b.e2];

                cn += strs[q.a.a2];
                cn += strs[q.a.s2];
                cn += strs[q.a.d2];
                cn += " ~";
                cn += strs[q.b.a2];
                cn += strs[q.b.s2];
                cn += strs[q.b.d2];

                dw += strs[q.a.z2];
                dw += strs[q.a.x2];
                dw += strs[q.a.c2];
                dw += "  ";
                dw += strs[q.b.z2];
                dw += strs[q.b.x2];
                dw += strs[q.b.c2];

                imgui_str(up + " \n" + cn + " \n" + dw + " ");
            };
            */
            bool sel = false;

            // TODO: recheck id & tid logic... (& imagebutton)
            auto check = [&, id = 0, r = ImGui::GetFrameHeight()](termT& term) mutable {
                // TODO: which should come first? rendering or dummy button?
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                const ImVec2 pos_max = pos + ImVec2{r, r};
                // TODO: a bit ugly...
                ImGui::GetWindowDrawList()->AddRectFilled(pos, pos_max,
                                                          term.selected  ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
                                                          : term.covered ? ImGui::GetColorU32(ImGuiCol_FrameBg)
                                                                         : 0);
                ImGui::GetWindowDrawList()->AddRect(pos, pos_max, ImGui::GetColorU32(ImGuiCol_Button));

                // TODO: Flip...
                if (satisfies(target, {}, term.eq)) {
                    ImGui::GetWindowDrawList()->AddRect(pos, pos_max, ImGui::GetColorU32(ImVec4{0, 1, 0, 1}));
                }

                ImGui::PushID(id++);
                bool hit = ImGui::InvisibleButton("Check", ImVec2{r, r});
                // TODO: this is in imgui_internal.h...
                // TODO: Ask is it intentional to make InvisibleButton highlight-less?
                // TODO: use normal buttons instead?
                ImGui::RenderNavHighlight({ImGui::GetItemRectMin(), ImGui::GetItemRectMax()}, ImGui::GetItemID());
                ImGui::PopID();

                // if (ImGui::BeginItemTooltip()) {
                //     show_pair(term.eq);
                //     ImGui::EndTooltip();
                // }
                if (hit) {
                    term.selected = !term.selected;
                    sel = true;
                }
            };

            // TODO: slightly confusing; light color should represent "take-into-account" instead of "ignore"
            // Is this solvable by applying specific coloring scheme?
            ImGui::BeginGroup();
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (int l = 0; l < 3; ++l) {
                check(terms_ignore[l * 3 + 0]);
                ImGui::SameLine();
                check(terms_ignore[l * 3 + 1]);
                ImGui::SameLine();
                check(terms_ignore[l * 3 + 2]);
            }
            ImGui::PopStyleVar();
            ImGui::EndGroup();

            auto table = [&check, tid = 0](termT_vec& terms) mutable {
                ImGui::PushID(tid++);
                if (ImGui::BeginTable("Table", terms.size(), ImGuiTableFlags_BordersInner)) {
                    ImGui::TableNextRow();
                    for (termT& t : terms) {
                        ImGui::TableNextColumn();
                        imgui_str(t.msg);
                        check(t);
                    }
                    ImGui::EndTable();
                }
                ImGui::PopID();
            };

            table(terms_native);
            ImGui::Separator();
            table(terms_misc);
            ImGui::Separator();
            table(terms_hex);

            if (sel) {
                reset_par();
            }
            return *par;
        }
    };
} // namespace legacy

void show_target_rule(const legacy::ruleT& target, rule_recorder& recorder) {
    std::string rule_str = to_MAP_str(target);

    ImGui::AlignTextToFramePadding();
    imgui_str("[Current rule]");
    ImGui::SameLine();
    if (ImGui::Button("Copy")) {
        ImGui::SetClipboardText(rule_str.c_str());
        logger::log_temp(300ms, "Copied");
    }
    ImGui::SameLine();
    if (ImGui::Button("Paste")) {
        // TODO: redesign... (especially, should not replace directly?)
        if (const char* text = ImGui::GetClipboardText()) {
            auto result = extract_rules(text);
            // TODO: copied from the main function...
            if (!result.empty()) {
                logger::log_temp(500ms, "found {} rules", result.size());
                recorder.replace(std::move(result));
            } else {
                logger::log_temp(300ms, "found nothing");
            }
        }
        // else...
    }

#if 0
    // TODO: is random-access useful?
    static char buf_pos[20]{};
    const auto filter = [](ImGuiInputTextCallbackData* data) -> int {
        return (data->EventChar >= '0' && data->EventChar <= '9') ? 0 : 1;
    };
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputTextWithHint("##Goto", "GOTO e.g. 2->enter", buf_pos, 20,
                                 ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_EnterReturnsTrue,
                                 filter)) {
        int val{};
        if (std::from_chars(buf_pos, buf_pos + strlen(buf_pos), val).ec == std::errc{}) {
            recorder.set_pos(val - 1); // TODO: -1 is clumsy.
        }
        buf_pos[0] = '\0';

        // Regain focus:
        ImGui::SetKeyboardFocusHere(-1);
    }
#endif

    // TODO: re-implement file-saving
    imgui_str(rule_str);
}

// TODO: too shaky...
std::optional<legacy::lockT> temp_lock{};

// TODO: should be a class... how to decouple? ...
void edit_rule(const legacy::ruleT& target, const code_image& icons, rule_recorder& recorder) {
    static legacy::interT inter = {};
    {
        int itag = inter.tag;
        const auto tooltip = [](legacy::interT::tagE tag, const char* msg = "View from:") {
            if (ImGui::BeginItemTooltip()) {
                imgui_str(msg);
                ImGui::PushTextWrapPos(250); // TODO: how to decide wrap pos properly?
                imgui_str(to_MAP_str(inter.get_viewer(tag)));
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
        };

        // TODO: better name (e.g. might be better named "direct")
        ImGui::AlignTextToFramePadding();
        imgui_str("View"); // TODO: rename vars...
        ImGui::SameLine();
        ImGui::RadioButton("Val", &itag, inter.Value);
        tooltip(inter.Value);
        ImGui::SameLine();
        ImGui::RadioButton("Flp", &itag, inter.Flip);
        tooltip(inter.Flip);
        ImGui::SameLine();
        ImGui::RadioButton("Dif", &itag, inter.Diff);
        tooltip(inter.Diff);

        inter.tag = legacy::interT::tagE{itag};
        if (inter.tag == inter.Diff) {
            ImGui::SameLine();
            if (ImGui::Button("Take current rule")) {
                inter.custom = target;
            }
            tooltip(inter.Diff);
        } else {
            // TODO: demonstration-only (this function should be explicit). Redesign...
            ImGui::SameLine();
            if (ImGui::Button("Try diff")) {
                inter.tag = inter.Diff;
            }
        }
    }

    // TODO: rename...
    static legacy::partition_collection parcol;
    const auto& part = parcol.get_par(target);
    const int k = part.k();

    static legacy::lockT locked{};
    if (temp_lock) {
        locked = *temp_lock;
        temp_lock.reset();
    }
    {
        // TODO: still unstable between partition switches...
        // TODO: the range should be scoped by locks... so, what should rcount be?
        static int rcount = 0.5 * k;
        const int freec = legacy::get_free_indexes(locked, part).size(); // TODO: wasteful...
        // TODO: refine...
        rcount = std::clamp(rcount, 0, freec);

        // ~ referred to InputScalar...
        // TODO: 200->global constant...
        const float r = ImGui::GetFrameHeight();
        const float s = ImGui::GetStyle().ItemInnerSpacing.x;
        ImGui::SetNextItemWidth(200 - 2 * (r + s)); // TODO: underflow? (200 is a bit short...)
        ImGui::SliderInt("##Slider", &rcount, 0, k, "%d", ImGuiSliderFlags_NoInput);
        ImGui::PushButtonRepeat(true);
        ImGui::SameLine(0, s);
        if (ImGui::Button("-", ImVec2(r, r))) {
            rcount = std::max(0, rcount - 1);
        }
        ImGui::SameLine(0, s);
        if (ImGui::Button("+", ImVec2(r, r))) {
            rcount = std::min(freec, rcount + 1);
        }
        ImGui::PopButtonRepeat();

        ImGui::SameLine();
        if (ImGui::Button("Randomize") || imgui_keypressed(ImGuiKey_Enter, false)) {
            recorder.take(random_flip(inter.get_viewer(), part, locked, target, rcount, rcount,
                                      global_mt19937)); // TODO: range...
        }

        // TODO: redesign...
        const auto iter_pair = [](const char* tag_first, const char* tag_prev, const char* tag_next,
                                  const char* tag_last, auto act_first, auto act_prev, auto act_next, auto act_last) {
            if (ImGui::Button(tag_first)) {
                act_first();
            }

            ImGui::SameLine();
            ImGui::BeginGroup();
            if (ImGui::Button(tag_prev)) {
                act_prev();
            }
            ImGui::SameLine(0, 2);
            if (ImGui::Button(tag_next)) {
                act_next();
            }
            ImGui::EndGroup();
            if (ImGui::IsItemHovered()) {
                if (imgui_scrollup()) {
                    act_prev();
                } else if (imgui_scrolldown()) {
                    act_next();
                }
            }

            ImGui::SameLine();
            if (ImGui::Button(tag_last)) {
                act_last();
            }
        };

        ImGui::Separator();
        // TODO: +1 is clumsy
        ImGui::AlignTextToFramePadding();
        imgui_str(std::format("Total:{} At:{}", recorder.size(), recorder.pos() + 1));
        ImGui::SameLine();
        iter_pair(
            "<|", "prev", "next", "|>", //
            [&] { recorder.set_first(); }, [&] { recorder.prev(); }, [&] { recorder.next(); },
            [&] { recorder.set_last(); });
        ImGui::Separator();

        iter_pair(
            "<00..", "dec", "inc", "11..>", //
            [&] { recorder.take(legacy::act_int::first(inter, part, target, locked)); },
            [&] { recorder.take(legacy::act_int::prev(inter, part, target, locked)); },
            [&] { recorder.take(legacy::act_int::next(inter, part, target, locked)); },
            [&] { recorder.take(legacy::act_int::last(inter, part, target, locked)); });
        ImGui::SameLine(), imgui_str("|");
        ImGui::SameLine();
        iter_pair(
            "<1.0.", "pprev", "pnext", "0.1.>", //
            [&] { recorder.take(legacy::act_perm::first(inter, part, target, locked)); },
            [&] { recorder.take(legacy::act_perm::prev(inter, part, target, locked)); },
            [&] { recorder.take(legacy::act_perm::next(inter, part, target, locked)); },
            [&] { recorder.take(legacy::act_perm::last(inter, part, target, locked)); });
        ImGui::SameLine(), imgui_str("|");
        ImGui::SameLine();
        if (ImGui::Button("Mir")) {
            recorder.replace_current(legacy::mirror(target));
        }
    }
    {
        // TODO: statistics...
        if (ImGui::Button("Enhance locks")) {
            for (int j = 0; j < part.k(); ++j) {
                const auto& group = part.jth_group(j);
                if (legacy::any_locked(locked, group)) {
                    for (auto code : group) {
                        locked[code] = true;
                    }
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear locks")) {
            locked = {};
        }
    }

    {
        static const char* const strss[3][3]{{"-0", "-1", "-x"}, //
                                             {"-.", "-f", "-x"},
                                             {"-.", "-d", "-x"}};
        const auto strs = strss[inter.tag];

        const legacy::ruleT_data drule = inter.from_rule(target);
        const legacy::scanlistT scans(part, drule);
        ImGui::Text("Groups:%d [%c:%d] [%c:%d] [%c:%d]", k, strs[0][1], scans.count(scans.A0), strs[1][1],
                    scans.count(scans.A1), strs[2][1], scans.count(scans.Inconsistent));

        const int zoom = 7;
        if (auto child = imgui_childwindow("Details")) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
            for (int j = 0; j < k; ++j) {
                if (j % 8 != 0) {
                    ImGui::SameLine();
                }
                if (j != 0 && j % 64 == 0) {
                    ImGui::Separator(); // TODO: refine...
                }
                const bool inconsistent = scans[j] == scans.Inconsistent;
                const auto& group = part.jth_group(j);
                const legacy::codeT head = group[0];
                const bool has_lock = legacy::any_locked(locked, group);

                if (inconsistent) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0, 0, 1));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0, 0, 1));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0, 0, 1));
                }
                const bool button_hit = icons.button(head, zoom);
                const bool button_hover = ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip);
                ImGui::SameLine();
                ImGui::AlignTextToFramePadding();
                imgui_str(strs[drule[head]]);

                if (has_lock) {
                    // TODO: -> widget func... (addborder)
                    const ImU32 col = legacy::all_locked(locked, group) ? -1 : 0xaaaaaaaa;
                    ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin() - ImVec2(2, 2),
                                                        ImGui::GetItemRectMax() + ImVec2(2, 2), col);
                }
                if (inconsistent) {
                    ImGui::PopStyleColor(3);
                }

                if (button_hover && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    for (auto code : group) {
                        locked[code] = !has_lock; // TODO: not reversible; is this ok?
                    }
                }
                static bool show_group = true;
                ImGui::SameLine();
                imgui_strdisabled("?");
                if (ImGui::IsItemHovered()) {
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                        show_group = !show_group;
                    }
                    if (show_group && ImGui::BeginTooltip()) {
                        for (int x = 0; auto code : group) {
                            if (x++ % 8 != 0) {
                                ImGui::SameLine();
                            }
                            // TODO: change color?
                            // TODO: what is tint_col?
                            icons.image(code, zoom, ImVec4(1, 1, 1, 1), ImVec4(0.5, 0.5, 0.5, 1));
                            ImGui::SameLine();
                            ImGui::AlignTextToFramePadding();
                            imgui_str(strs[drule[code]]);
                            if (locked[code]) {
                                ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin() - ImVec2(2, 2),
                                                                    ImGui::GetItemRectMax() + ImVec2(2, 2), -1);
                            }
                        }
                        ImGui::EndTooltip();
                    }
                }
                if (button_hit) {
                    // TODO: document this behavior... (keyctrl->resolve conflicts)
                    legacy::ruleT r = target;
                    if (ImGui::GetIO().KeyCtrl) {
                        legacy::copy(group, inter.get_viewer(), r);
                    } else {
                        legacy::flip(group, r);
                    }
                    recorder.take(r); // replace_current?
                }
            }
            ImGui::PopStyleVar(2);
        }
    }
}

// TODO: refine...
// TODO: able to create/append/open file?
class file_navT {
    using path = std::filesystem::path;
    using clock = std::chrono::steady_clock;

    static path _u8path(const char* str) {
        assert(str);
        // TODO: silence the warning. The deprecation is very stupid.
        // TODO: start_lifetime (new(...)) as char8_t?
        return std::filesystem::u8path(str);
    }

    static path exe_path() {
        const std::unique_ptr<char[], decltype(+SDL_free)> p(SDL_GetBasePath(), SDL_free);
        assert(p); // TODO: what if this fails? what if exe path is invalid?
        return _u8path(p.get());
    }

    char buf_path[200]{};
    char buf_filter[20]{".txt"};

    std::vector<std::filesystem::directory_entry> dirs;
    std::vector<std::filesystem::directory_entry> files;

    clock::time_point expired = {};

    void set_current(const path& p) {
        // (Catching eagerly to avoid some flickering...)
        try {
            std::filesystem::current_path(p);
            expired = {};
        } catch (const std::exception& what) {
            // TODO: what encoding?
            logger::log_temp(1000ms, "Exception:\n{}", what.what());
        }
    }
    void refresh() {
        // Setting outside of try-block to avoid too frequent failures...
        // TODO: log_temp is global; can be problematic...
        expired = std::chrono::steady_clock::now() + 3000ms;
        try {
            dirs.clear();
            files.clear();
            for (const auto& entry : std::filesystem::directory_iterator(
                     std::filesystem::current_path(), std::filesystem::directory_options::skip_permission_denied)) {
                const auto status = entry.status();
                if (is_regular_file(status)) {
                    files.emplace_back(entry);
                }
                if (is_directory(status)) {
                    dirs.emplace_back(entry);
                }
            }
        } catch (const std::exception& what) {
            // TODO: what encoding?
            logger::log_temp(1000ms, "Exception:\n{}", what.what());
        }
    }

public:
    [[nodiscard]] std::optional<path> window(const char* id_str, bool* p_open) {
        auto window = imgui_window(id_str, p_open);
        if (!window) {
            return std::nullopt;
        }

        using namespace std;
        using namespace std::filesystem;
        optional<path> target = nullopt;

        imgui_strwrapped(cpp17_u8string(current_path()));
        ImGui::Separator();

        if (ImGui::BeginTable("##Table", 2, ImGuiTableFlags_Resizable)) {
            if (clock::now() > expired) {
                refresh();
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            {
                if (ImGui::InputText("Path", buf_path, std::size(buf_path), ImGuiInputTextFlags_EnterReturnsTrue)) {
                    set_current(_u8path(buf_path));
                    buf_path[0] = '\0';
                }
                if (ImGui::MenuItem("-> Exe path")) {
                    set_current(exe_path());
                }
                if (ImGui::MenuItem("-> ..")) {
                    set_current("..");
                }
                ImGui::Separator();
                if (auto child = imgui_childwindow("Folders")) {
                    if (dirs.empty()) {
                        imgui_strdisabled("None");
                    }
                    for (const auto& entry : dirs) {
                        // TODO: cache str?
                        const auto str = cpp17_u8string(entry.path().filename());
                        if (ImGui::Selectable(str.c_str())) {
                            // Won't affect the loop at this frame.
                            set_current(entry.path());
                        }
                    }
                }
            }
            ImGui::TableNextColumn();
            {
                ImGui::InputText("Filter", buf_filter, std::size(buf_filter));
                ImGui::Separator();
                if (auto child = imgui_childwindow("Files")) {
                    bool has = false;
                    for (const auto& entry : files) {
                        const auto str = cpp17_u8string(entry.path().filename());
                        if (str.find(buf_filter) != str.npos) {
                            has = true;
                            if (ImGui::Selectable(str.c_str())) {
                                target = entry.path();
                            }
                        }
                    }
                    if (!has) {
                        imgui_strdisabled("None");
                    }
                }
            }
            ImGui::EndTable();
        }

        return target;
    }
};

// TODO: should enable guide-mode (with a switch...)
// TODO: changes upon the same rule should be grouped together... how? (editor++)...
// TODO: how to capture certain patterns? (editor++)...

// TODO: reconsider: where should "current-rule" be located...
struct runner_ctrl {
    legacy::ruleT rule;

    static constexpr int pergen_min = 1, pergen_max = 20;
    int pergen = 1;
    bool anti_flick = true; // TODO: add explanation
    int actual_pergen() const {
        if (anti_flick && legacy::will_flick(rule) && pergen % 2) {
            return pergen + 1;
        }
        return pergen;
    }

    static constexpr int start_min = 0, start_max = 1000;
    int start_from = 0;

    static constexpr int gap_min = 0, gap_max = 20;
    int gap_frame = 0;

    bool pause = false;
    bool pause2 = false; // TODO: explain...

    void run(torusT& runner, int extra = 0) const {
        if (runner.gen() < start_from) {
            runner.run(rule, start_from - runner.gen());
        } else {
            if (extra != 0) {
                runner.run(rule, extra);
                extra = 0;
            }
            if (!pause && !pause2) {
                if (ImGui::GetFrameCount() % (gap_frame + 1) == 0) {
                    runner.run(rule, actual_pergen());
                }
            }
        }
    }
};

int main(int argc, char** argv) {
    app_backend::init();

    {
        // Avoid "imgui.ini" (and maybe also "imgui_log.txt") sprinking everywhere.
        // TODO: As to "strdup": temporary; the leaks are negligible here.
        // TODO: eh... strdup is not always available...
        // https://en.cppreference.com/w/c/experimental/dynamic/strdup
        // TODO: maybe setting to SDL_GetBasePath / ...
        const auto cur = std::filesystem::current_path();
        ImGui::GetIO().IniFilename = strdup(cpp17_u8string(cur / "imgui.ini").c_str());
        ImGui::GetIO().LogFilename = strdup(cpp17_u8string(cur / "imgui_log.txt").c_str());

        // TODO: remove when finished...
        std::filesystem::current_path(R"(C:\*redacted*\Desktop\rulelists_new)");
    }

    // ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    // ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // TODO: works but blurry, and how to apply in project?
    // {
    //     const char* fnpath = R"(C:\*redacted*\Desktop\Deng.ttf)";
    //     ImGui::GetIO().Fonts->AddFontFromFileTTF(fnpath, 13, nullptr,
    //                                                ImGui::GetIO().Fonts->GetGlyphRangesChineseFull());
    // }

    rule_recorder recorder;
    // TODO: what encoding?
    if (argc == 2) {
        recorder.replace(read_rule_from_file(argv[1]));
        // TODO: set as working path when the path is valid?
        // TODO: what if the path is invalid?
    }

    // TODO: redesign...
    tileT_fill_arg filler{.use_seed = true, .seed = 0, .density = 0.5};
    torusT runner({.width = 480, .height = 360});
    runner.restart(filler);

    runner_ctrl ctrl{
        .rule = recorder.current(), .pergen = 1, .anti_flick = true, .start_from = 0, .gap_frame = 0, .pause = false};

    bool show_nav_window = true;
    file_navT nav;

    tile_image img(runner.tile());
    code_image icons;
    while (app_backend::new_frame()) {
        // TODO: applying following logic; consider refining it.
        // recorder is modified during display, but will synchronize with runner's before next frame.
        assert(ctrl.rule == recorder.current());

        if (show_nav_window) {
            if (auto sel = nav.window("File Nav", &show_nav_window)) {
                // TODO: record filename?
                auto result = read_rule_from_file(*sel);
                if (!result.empty()) {
                    logger::log_temp(500ms, "found {} rules", result.size());
                    recorder.replace(std::move(result));
                } else {
                    logger::log_temp(300ms, "found nothing");
                }
            }
        }

        const auto show_tile = [&] {
            // TODO: move elsewhere in the gui?
            ImGui::Text("Width:%d,Height:%d,Gen:%d,Density:%.4f", runner.tile().width(), runner.tile().height(),
                        runner.gen(), float(legacy::count(runner.tile())) / runner.tile().area());

            const bool corner = ImGui::Button("Corner"); // TODO: move elsewhere...
            ImGui::SameLine();
            const bool center = ImGui::Button("Center");
            // TODO: resize-fullscreen...

            const int tile_width = runner.tile().width();
            const int tile_height = runner.tile().height();

            // TODO: rename to canvas_pos/size?
            const ImVec2 screen_pos = ImGui::GetCursorScreenPos();
            const ImVec2 screen_size = ImGui::GetContentRegionAvail();
            ImDrawList& drawlist = *ImGui::GetWindowDrawList();

            drawlist.PushClipRect(screen_pos, screen_pos + screen_size);
            drawlist.AddRectFilled(screen_pos, screen_pos + screen_size, IM_COL32(20, 20, 20, 255));

            // TODO: int?
            static float zoom = 1; // TODO: mini window when zoom == 1?
            // It has been proven that `img_off` works better than using `corner_idx` (cell idx in the corner)
            static ImVec2 img_off = {0, 0}; // TODO: supposed to be of integer-precision...
            if (corner) {
                img_off = {0, 0};
            }
            if (center) {
                img_off = screen_size / 2 - ImVec2(tile_width / 2, tile_height / 2) * zoom;
                img_off.x = floor(img_off.x);
                img_off.y = floor(img_off.y);
            }

            const ImVec2 img_pos = screen_pos + img_off;
            img.update(runner.tile());
            drawlist.AddImage(img.texture(), img_pos, img_pos + ImVec2(img.width(), img.height()) * zoom);
            // Experimental: select:
            // TODO: this shall belong to the runner.
            static ImVec2 select_0{}, select_1{}; // tile index, not pixel.
            // TODO: shaky...
            // TODO: show selected size...
            // TODO: ctrl to move selected area?
            struct sel_info {
                int x1, y1; // [
                int x2, y2; // )

                int width() const { return x2 - x1; }
                int height() const { return y2 - y1; }
                ImVec2 min() const { return ImVec2(x1, y1); }
                ImVec2 max() const { return ImVec2(x2, y2); }
                explicit operator bool() const { return width() > 1 || height() > 1; }
            };
            const auto get_select = []() -> sel_info {
                // TODO: rephrase...
                // select_0/1 denotes []; convert to [):
                int x1 = select_0.x, x2 = select_1.x;
                int y1 = select_0.y, y2 = select_1.y;
                if (x1 > x2) {
                    std::swap(x1, x2);
                }
                if (y1 > y2) {
                    std::swap(y1, y2);
                }
                return {x1, y1, x2 + 1, y2 + 1};
            };
            // TODO: whether to limit this way? (maybe better to check click pos instead...)
            if (sel_info sel = get_select()) {
                drawlist.AddRectFilled(img_pos + sel.min() * zoom, img_pos + sel.max() * zoom, IM_COL32(0, 255, 0, 60));
            }
            drawlist.PopClipRect();

            ImGui::InvisibleButton("Canvas", screen_size);
            const bool active = ImGui::IsItemActive();
            ctrl.pause2 = active;
            if (ImGui::IsItemHovered()) {
                const ImGuiIO& io = ImGui::GetIO();

                assert(ImGui::IsMousePosValid());
                // It turned out that, this will work well even if outside of the image...
                const ImVec2 mouse_pos = io.MousePos;
                if (active) {
                    // TODO: whether to support shifting at all?
                    if (!io.KeyCtrl) {
                        img_off += io.MouseDelta;
                    } else {
                        // TODO: this approach is highly imprecise when zoom != 1, but does this matter?
                        runner.shift(io.MouseDelta.x / zoom, io.MouseDelta.y / zoom);
                    }
                }
                // TODO: rename to mouseXXX...
                if (imgui_scrolling()) {
                    ImVec2 cellidx = (mouse_pos - img_pos) / zoom;
                    if (imgui_scrolldown() && zoom != 1) { // TODO: 0.5?
                        zoom /= 2;
                    }
                    if (imgui_scrollup() && zoom != 8) {
                        zoom *= 2;
                    }
                    img_off = (mouse_pos - cellidx * zoom) - screen_pos;
                    img_off.x = round(img_off.x);
                    img_off.y = round(img_off.y); // TODO: is rounding correct?
                }

                // Experimental: select:
                // TODO: this shall belong to the runner.
                // TODO: precedence against left-clicking?
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    // ctrl.pause = true;
                    int celx = floor((mouse_pos.x - img_pos.x) / zoom);
                    int cely = floor((mouse_pos.y - img_pos.y) / zoom);

                    celx = std::clamp(celx, 0, tile_width - 1);
                    cely = std::clamp(cely, 0, tile_height - 1);
                    select_0 = ImVec2(celx, cely);
                }
                if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    int celx = floor((mouse_pos.x - img_pos.x) / zoom);
                    int cely = floor((mouse_pos.y - img_pos.y) / zoom);

                    celx = std::clamp(celx, 0, tile_width - 1);
                    cely = std::clamp(cely, 0, tile_height - 1);
                    select_1 = ImVec2(celx, cely);
                }
            }
            if (sel_info sel = get_select()) {
                // TODO: paste?
                if (imgui_keypressed(ImGuiKey_C, false)) {
                    legacy::tileT t({.width = sel.width(), .height = sel.height()});
                    legacy::copy(runner.tile(), sel.x1, sel.y1, sel.width(), sel.height(), t, 0, 0);
                    std::string str = std::format("x = {}, y = {}, rule = {}\n{}", t.width(), t.height(),
                                                  legacy::to_MAP_str(ctrl.rule), legacy::to_rle_str(t));
                    ImGui::SetClipboardText(str.c_str());
                }
                // TODO: rand-mode (whether reproducible...)
                // TODO: clear mode (random/all-0,all-1/paste...) / (clear inner/outer)
                // TODO: horrible; redesign (including control) ...
                // TODO: 0/1/other textures are subject to agar settings...
                if (imgui_keypressed(ImGuiKey_Backspace, false)) {
                    legacy::tileT& tile = const_cast<legacy::tileT&>(runner.tile());
                    for (int y = sel.y1; y < sel.y2; ++y) {
                        for (int x = sel.x1; x < sel.x2; ++x) {
                            tile.line(y)[x] = 0;
                        }
                    }
                }
                if (imgui_keypressed(ImGuiKey_Equal, false)) {
                    legacy::tileT& tile = const_cast<legacy::tileT&>(runner.tile());
                    constexpr uint32_t c = std::mt19937::max() * 0.5;
                    for (int y = sel.y1; y < sel.y2; ++y) {
                        for (int x = sel.x1; x < sel.x2; ++x) {
                            tile.line(y)[x] = global_mt19937() < c;
                        }
                    }
                }
                // TODO: refine capturing...
                if (imgui_keypressed(ImGuiKey_P, false)) {
                    // TODO: padding area...
                    const legacy::rectT size = {.width = sel.width(), .height = sel.height()};
                    legacy::tileT cap(size), cap2(size);
                    for (int y = sel.y1; y < sel.y2; ++y) {
                        for (int x = sel.x1; x < sel.x2; ++x) {
                            cap.line(y - sel.y1)[x - sel.x1] = runner.tile().line(y)[x];
                        }
                    }
                    legacy::lockT locked{};
                    auto rulx = [&](legacy::codeT code) {
                        locked[code] = true;
                        return ctrl.rule(code);
                    };
                    for (int g = 0; g < 50; ++g) {
                        cap.gather(cap, cap, cap, cap, cap, cap, cap, cap);
                        cap.apply(rulx, cap2);
                        cap.swap(cap2);
                    }
                    temp_lock.emplace(locked);
                }
            }
        };

        // TODO: not robust
        // ~ runner.restart(...) shall not happen before rendering.
        bool restart = false;
        int extra = 0;

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        if (auto window = imgui_window("Tile", flags)) {
            // ImGui::Checkbox("Log window", &show_log_window);
            // ImGui::SameLine();
            ImGui::Checkbox("Nav window", &show_nav_window);
            ImGui::SameLine();
            ImGui::Text("   (%.1f FPS) Frame:%d\n", ImGui::GetIO().Framerate, ImGui::GetFrameCount());

            show_target_rule(ctrl.rule, recorder);
            ImGui::Separator();

            if (ImGui::BeginTable("Layout", 2, ImGuiTableFlags_Resizable)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (auto child = imgui_childwindow("Rul")) {
                    edit_rule(ctrl.rule, icons, recorder);
                }
                ImGui::TableNextColumn();
                if (auto child = imgui_childwindow("Til")) {
                    ImGui::PushItemWidth(200); // TODO: flexible...
                    ImGui::BeginGroup();
                    {
                        ImGui::Checkbox("Pause", &ctrl.pause);
                        ImGui::SameLine();
                        ImGui::BeginDisabled();
                        ImGui::Checkbox("Pause2", &ctrl.pause2);
                        ImGui::EndDisabled();
                        ImGui::SameLine();
                        // ↑ TODO: better visual?
                        // ↓ TODO: imgui_repeatbutton?
                        ImGui::PushButtonRepeat(true);
                        // TODO: should allow keyboard control...
                        if (ImGui::Button("+1")) {
                            extra = 1;
                            // logger::log_temp(200ms, "+1"); // TODO: useful?
                        }
                        ImGui::SameLine();
                        // TODO: is this usage of ### correct?
                        if (ImGui::Button(std::format("+p({})###+p", ctrl.actual_pergen()).c_str())) {
                            extra = ctrl.actual_pergen();
                            // logger::log_temp(200ms, "+p({})", ctrl.actual_pergen());
                        }
                        ImGui::PopButtonRepeat();
                        ImGui::SameLine();
                        if (ImGui::Button("Restart") || imgui_keypressed(ImGuiKey_R, false)) {
                            restart = true;
                        }

                        // TODO: Gap-frame shall be really timer-based...
                        ImGui::SliderInt("Gap Frame [0-20]", &ctrl.gap_frame, ctrl.gap_min, ctrl.gap_max, "%d",
                                         ImGuiSliderFlags_NoInput);
                        ImGui::SliderInt("Start gen [0-1000]", &ctrl.start_from, ctrl.start_min, ctrl.start_max, "%d",
                                         ImGuiSliderFlags_NoInput);
                        ImGui::SliderInt("Pergen [1-20]", &ctrl.pergen, ctrl.pergen_min, ctrl.pergen_max, "%d",
                                         ImGuiSliderFlags_NoInput);
                        ImGui::AlignTextToFramePadding();
                        ImGui::Text("(Actual pergen: %d)", ctrl.actual_pergen());
                        ImGui::SameLine();
                        ImGui::Checkbox("anti-flick", &ctrl.anti_flick);
                    }
                    ImGui::EndGroup();
                    ImGui::SameLine();
                    ImGui::BeginGroup();
                    {
                        // TODO: use radio instead?
                        if (ImGui::Checkbox("Use seed", &filler.use_seed)) {
                            // TODO: unconditional?
                            if (filler.use_seed) {
                                restart = true;
                            }
                        }
                        if (!filler.use_seed) {
                            ImGui::BeginDisabled();
                        }
                        // TODO: uint32_t...
                        // TODO: want resetting only when +/-/enter...
                        int seed = filler.seed;
                        // TODO: same as "rule_editor"'s... but don't want to affect Label...
                        if (ImGui::InputInt("Seed", &seed)) {
                            seed = std::clamp(seed, 0, 9999);
                            if (seed != filler.seed) {
                                filler.seed = seed;
                                restart = true;
                            }
                        }
                        if (!filler.use_seed) {
                            ImGui::EndDisabled();
                        }

                        // TODO: integer(ratio) density?
                        if (ImGui::SliderFloat("Init density [0-1]", &filler.density, 0.0f, 1.0f, "%.3f",
                                               ImGuiSliderFlags_NoInput)) {
                            restart = true;
                        }
                    }
                    ImGui::EndGroup();
                    ImGui::PopItemWidth();

                    // TODO: shall redesign...
                    // TODO: enable/disable keyboard ctrl (enable by default)
                    // TODO: redesign keyboard ctrl...
                    if (imgui_keypressed(ImGuiKey_1, true)) {
                        ctrl.gap_frame = std::max(ctrl.gap_min, ctrl.gap_frame - 1);
                    }
                    if (imgui_keypressed(ImGuiKey_2, true)) {
                        ctrl.gap_frame = std::min(ctrl.gap_max, ctrl.gap_frame + 1);
                    }
                    if (imgui_keypressed(ImGuiKey_3, true)) {
                        ctrl.pergen = std::max(ctrl.pergen_min, ctrl.pergen - 1);
                    }
                    if (imgui_keypressed(ImGuiKey_4, true)) {
                        ctrl.pergen = std::min(ctrl.pergen_max, ctrl.pergen + 1);
                    }
                    // TODO: want to allow setting hard pausing when dragging (soft locking)...
                    if (imgui_keypressed(ImGuiKey_Space, true)) {
                        ctrl.pause = !ctrl.pause;
                    }
                    // TODO: temp (this function turns out to be necessary...)
                    if (imgui_keypressed(ImGuiKey_M, true)) {
                        if (ctrl.pause) {
                            extra = ctrl.actual_pergen();
                        }
                        ctrl.pause = true;
                    }

                    show_tile();
                }
                ImGui::EndTable();
            }
        }

        ImGui::ShowDemoWindow(); // TODO: remove (or comment-out) this when all done...
        logger::tempwindow();
        app_backend::render(); // !!! TODO: recheck

        if (ctrl.rule != recorder.current()) {
            ctrl.rule = recorder.current();
            restart = true;
            ctrl.pause = false; // TODO: this should be configurable...
        }
        if (restart) {
            runner.restart(filler);
        }
        ctrl.run(runner, extra);
    }

    app_backend::clear();
    return 0;
}
