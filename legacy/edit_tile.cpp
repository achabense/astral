#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "app.hpp"

// TODO: explain...
// #define ENABLE_START_GEN

// TODO (temp) there was a `use_seed` in the class.
// It is useless, as to get non-repeatable data range-select based fill can do exactly the same thing.

// About float: there is only ImGui::SliderFloat, so use float for convenience.
// TODO: define imgui_sliderdouble in app_imgui.hpp?
struct initT {
    legacy::tileT::sizeT size;
    uint32_t seed;
    float density; // ∈ [0.0f, 1.0f]
};

class torusT {
    legacy::tileT m_tile, m_side;
    int m_gen;

public:
    explicit torusT(const initT& init) : m_tile(init.size), m_side(init.size), m_gen(0) { restart(init); }

    // TODO: reconsider whether to expose non-const tile...
    legacy::tileT& tile() { return m_tile; }
    const legacy::tileT& tile() const { return m_tile; }
    int gen() const { return m_gen; }

    void restart(const initT& init) {
        m_tile.resize(init.size);
        m_side.resize(init.size);

        std::mt19937 rand(init.seed);
        legacy::random_fill(m_tile, rand, init.density);

        m_gen = 0;
    }

    void run(const legacy::ruleT& rule, int count = 1) {
        for (int c = 0; c < count; ++c) {
            m_tile.gather(m_tile, m_tile, m_tile, m_tile, m_tile, m_tile, m_tile, m_tile);
            m_tile.apply(rule, m_side);
            m_tile.swap(m_side);

            ++m_gen;
        }
    }

    // TODO: recheck logic...
    void shift(int dx, int dy) {
        const int width = m_tile.width(), height = m_tile.height();

        // TODO: proper name... (`wrap` seems a good one, e.g. unsigned's wrapping behavior)
        const auto round_clip = [](int v, int r) { return ((v % r) + r) % r; };
        dx = round_clip(-dx, width);
        dy = round_clip(-dy, height);
        if (dx == 0 && dy == 0) {
            return;
        }

        assert(m_side.size() == m_tile.size());
        for (int y = 0; y < height; ++y) {
            const bool* source = m_tile.line((y + dy) % height);
            bool* dest = m_side.line(y);
            std::copy_n(source, width, dest);
            std::rotate(dest, dest + dx, dest + width);
        }
        m_tile.swap(m_side);
    }
};

struct ctrlT {
    legacy::ruleT rule{};

    // TODO: better name?
    static constexpr int pace_min = 1, pace_max = 20;
    int pace = 1;
    bool anti_flick = true; // TODO: add explanation
    int actual_pace() const {
        if (anti_flick && legacy::will_flick(rule) && pace % 2) {
            return pace + 1;
        }
        return pace;
    }

#ifdef ENABLE_START_GEN
    static constexpr int start_min = 0, start_max = 200;
    int start_from = 0;
#endif // ENABLE_START_GEN

    // TODO: redesign?
    static constexpr int gap_min = 0, gap_max = 20;
    int gap_frame = 0;

    bool pause = false;
    bool pause2 = false; // TODO: explain...

    void run(torusT& runner, int extra = 0) const {
#ifdef ENABLE_START_GEN
        if (runner.gen() < start_from) {
            runner.run(rule, start_from - runner.gen());
            return;
        }
#endif // ENABLE_START_GEN

        if (extra != 0) {
            runner.run(rule, extra);
            extra = 0;
        }
        if (!pause && !pause2) {
            if (ImGui::GetFrameCount() % (gap_frame + 1) == 0) {
                runner.run(rule, actual_pace());
            }
        }
    }
};

// TODO: explain...
struct selectT {
    // []
    legacy::tileT::posT select_0{0, 0}, select_1{0, 0}; // cell index, not pixel.

    void clear() { select_0 = select_1 = {0, 0}; }
    void toggle_select_all(legacy::tileT::sizeT size) {
        // all-selected ? clear : select-all
        const auto [begin, end] = range();
        if (begin.x == 0 && begin.y == 0 && end.x == size.width && end.y == size.height) {
            clear();
        } else {
            select_0 = {0, 0};
            select_1 = {.x = size.width - 1, .y = size.height - 1};
        }
    }

    legacy::tileT::rangeT range() const {
        int x0 = select_0.x, x1 = select_1.x;
        int y0 = select_0.y, y1 = select_1.y;
        if (x0 > x1) {
            std::swap(x0, x1);
        }
        if (y0 > y1) {
            std::swap(y0, y1);
        }
        return {.begin{x0, y0}, .end{x1 + 1, y1 + 1}};
    }
};

// TODO: redesign...
void edit_tile(const legacy::ruleT& rule, legacy::lockT& locked, tile_image& img) {
    // TODO: (temp) these variables become static after moving code in main into this function...
    // which is not ideal...
    static initT init{.size{.width = 480, .height = 360}, .seed = 0, .density = 0.5};
    static torusT runner(init);
    assert(init.size == runner.tile().size());

    static ctrlT ctrl{.rule = rule, .pace = 1, .anti_flick = true, .gap_frame = 0, .pause = false};

    static ImVec2 img_off = {0, 0}; // TODO: supposed to be of integer-precision...
    static int img_zoom = 1;

    static std::optional<legacy::tileT> paste;
    static legacy::tileT::posT paste_beg{0, 0}; // dbegin for copy... (TODO: this is confusing...)

    static selectT sel{};

    // TODO: add a menu somewhere...

    // TODO: let right+ctrl move selected area?

    // TODO: set pattern as init state? what if size is already changed?
    // TODO: specify mouse-dragging behavior (especially, no-op must be an option)
    // TODO: range-selected randomization don't need fixed seed. However, there should be a way to specify density.
    // TODO: support drawing as a dragging behavior if easy.
    // TODO: copy vs copy to clipboard; paste vs paste from clipboard? (don't want to pollute clipboard with small
    // rle strings...
    // TODO: should be able to recognize "rule = " part in the rle string.

    // TODO: "periodical tile" feature is generally not too useful without boundless space, and torus is
    // enough for visual feedback.
    auto show_tile = [&] {
        // TODO: refine "resize" gui and logic...
        static char input_width[20]{}, input_height[20]{};
        const float s = ImGui::GetStyle().ItemInnerSpacing.x;
        {
            const auto filter = [](ImGuiInputTextCallbackData* data) -> int {
                return (data->EventChar >= '0' && data->EventChar <= '9') ? 0 : 1;
            };
            const float w = (ImGui::CalcItemWidth() - s) / 2;
            ImGui::SetNextItemWidth(w);
            ImGui::InputTextWithHint("##Width", "width", input_width, 20, ImGuiInputTextFlags_CallbackCharFilter,
                                     filter);
            ImGui::SameLine(0, s);
            ImGui::SetNextItemWidth(w);
            ImGui::InputTextWithHint("##Height", "height", input_height, 20, ImGuiInputTextFlags_CallbackCharFilter,
                                     filter);
            ImGui::SameLine(0, s);
        }
        const bool resize = ImGui::Button("Resize");

        // TODO: move elsewhere in the gui?
        ImGui::Text("Width:%d,Height:%d,Gen:%d,Density:%.4f", runner.tile().width(), runner.tile().height(),
                    runner.gen(), float(legacy::count(runner.tile())) / runner.tile().area());
        // TODO: canvas size, tile size, selected size, paste size...

        const bool corner = ImGui::Button("Corner"); // TODO: move elsewhere...
        ImGui::SameLine();
        const bool center = ImGui::Button("Center");
        ImGui::SameLine();
        imgui_str("Zoom");
        assert(img_zoom == 1 || img_zoom == 2 || img_zoom == 4 || img_zoom == 8);
        for (const int z : {1, 2, 4, 8}) {
            ImGui::SameLine(0, s);
            // TODO: avoid usage of format...
            if (ImGui::RadioButton(std::format("{}##Z{}", z, z).c_str(), img_zoom == z)) {
                img_off = {8, 8}; // TODO: temporarily intentional...
                img_zoom = z;
            }
        }

        ImGui::SameLine();
        const bool fit = ImGui::Button("Fit"); // TODO: size preview?
        // TODO: select all, unselect button...

        if (paste) {
            ImGui::SameLine(), imgui_str("|"), ImGui::SameLine();
            if (ImGui::Button("Drop paste")) {
                paste.reset();
            }
        }

        const ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        const ImVec2 canvas_size = ImGui::GetContentRegionAvail();

        // TODO: this can be negative wtf... investigate...
        // TODO: (regression?) some key ctrls are skipped by this...
        if (canvas_size.x <= 0 || canvas_size.y <= 0) {
            // TODO: show something when fit/etc are hit?
            ctrl.pause2 = true;
            // ImGui::Text("canvas: x=%f y=%f", canvas_size.x,canvas_size.y);
            return;
        }
        // TODO: or like this?
        // const ImVec2 canvas_size = [] {
        //     // Values of GetContentRegionAvail() can be negative...
        //     ImVec2 size = ImGui::GetContentRegionAvail();
        //     return ImVec2(std::max(size.x, 50.0f), std::max(size.y, 50.0f));
        // }();

        // TODO: the constraint is arbitrary; are there more sensible ways to decide size constraint?
        const auto size_clamped = [](int width, int height) {
            return legacy::tileT::sizeT{.width = std::clamp(width, 64, 1200), .height = std::clamp(height, 64, 1200)};
        };
        if (fit) {
            img_off = {0, 0};

            const legacy::tileT::sizeT size =
                size_clamped((int)canvas_size.x / img_zoom, (int)canvas_size.y / img_zoom);
            if (init.size != size) {
                init.size = size;
                runner.restart(init);
            }
        }
        if (resize) {
            // TODO: support using current screen/tilesize/zoom?
            int iwidth = 0, iheight = 0;
            const auto [ptr, ec] = std::from_chars(input_width, input_width + 20, iwidth);
            const auto [ptr2, ec2] = std::from_chars(input_height, input_height + 20, iheight);
            if (ec == std::errc{} && ec2 == std::errc{}) {
                img_off = {0, 0};
                img_zoom = 1; // <-- TODO: whether to reset zoom here?
                const legacy::tileT::sizeT size = size_clamped(iwidth, iheight);
                if (init.size != size) {
                    init.size = size;
                    runner.restart(init);
                }
            }
            // else ...
            input_width[0] = '\0';
            input_height[0] = '\0';
        }

        // Size is fixed now:
        const legacy::tileT::sizeT tile_size = runner.tile().size(); // TODO: which (vs init.size) is better?
        const ImVec2 img_size(tile_size.width * img_zoom, tile_size.height * img_zoom);

        // Validity of paste is fixed now:
        // This can happen when e.g. paste -> resize...
        if (paste && (paste->width() > tile_size.width || paste->height() > tile_size.height)) {
            paste.reset();
        }

        if (corner) {
            img_off = {0, 0};
        }
        if (center) {
            img_off = canvas_size / 2 - img_size / 2;
            img_off.x = floor(img_off.x);
            img_off.y = floor(img_off.y); // TODO: is flooring correct?
        }

        const ImVec2 img_pos = canvas_pos + img_off;

        ImDrawList* const drawlist = ImGui::GetWindowDrawList();
        drawlist->PushClipRect(canvas_pos, canvas_pos + canvas_size);
        drawlist->AddRectFilled(canvas_pos, canvas_pos + canvas_size, IM_COL32(20, 20, 20, 255));

        if (!paste) {
            drawlist->AddImage(img.update(runner.tile()), img_pos, img_pos + img_size);
        } else {
            // TODO: displays poorly with miniwindow...

            assert(paste->width() <= tile_size.width && paste->height() <= tile_size.height);
            paste_beg.x = std::clamp(paste_beg.x, 0, tile_size.width - paste->width());
            paste_beg.y = std::clamp(paste_beg.y, 0, tile_size.height - paste->height());
            // TODO: rename...
            const legacy::tileT::rangeT range = {paste_beg, paste_beg + paste->size()};

            legacy::tileT temp(paste->size()); // TODO: wasteful...
            legacy::copy(temp, {0, 0}, runner.tile(), range);
            legacy::copy<legacy::copyE::Or>(runner.tile(), paste_beg, *paste);

            drawlist->AddImage(img.update(runner.tile()), img_pos, img_pos + img_size);

            legacy::copy(runner.tile(), paste_beg, temp);

            const ImVec2 paste_min = img_pos + ImVec2(range.begin.x, range.begin.y) * img_zoom;
            const ImVec2 paste_max = img_pos + ImVec2(range.end.x, range.end.y) * img_zoom;
            drawlist->AddRectFilled(paste_min, paste_max, IM_COL32(255, 0, 0, 60));
        }

        if (const auto range = sel.range(); range.width() > 1 || range.height() > 1) {
            const ImVec2 sel_min = img_pos + ImVec2(range.begin.x, range.begin.y) * img_zoom;
            const ImVec2 sel_max = img_pos + ImVec2(range.end.x, range.end.y) * img_zoom;
            drawlist->AddRectFilled(sel_min, sel_max, IM_COL32(0, 255, 0, 40));
            drawlist->AddRect(sel_min, sel_max, IM_COL32(0, 255, 0, 160));
        }
        drawlist->PopClipRect();

        ImGui::InvisibleButton("Canvas", canvas_size);
        const bool active = ImGui::IsItemActive();
        ctrl.pause2 = paste || active; // TODO: won't work if |= active... should ctrl.run clear pause2?
        if (ImGui::IsItemHovered()) {
            // TODO: reorganize...
            const ImGuiIO& io = ImGui::GetIO();

            assert(ImGui::IsMousePosValid());
            // It turned out that, this will work well even if outside of the image...
            const ImVec2 mouse_pos = io.MousePos;
            if (active) {
                if (!io.KeyCtrl) {
                    img_off += io.MouseDelta;
                } else if (img_zoom == 1) {
                    runner.shift(io.MouseDelta.x, io.MouseDelta.y);
                }
            }

            // TODO: refine...
            // TODO: zoom window temporarily conflicts with range selection... (both use Rclick)
            if (!paste && img_zoom <= 2 && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                const int celx = floor((mouse_pos.x - img_pos.x) / img_zoom);
                const int cely = floor((mouse_pos.y - img_pos.y) / img_zoom);
                if (celx >= -10 && celx < tile_size.width + 10 && cely >= -10 && cely < tile_size.height + 10) {
                    static bool toggle = true;
                    if (auto tooltip = imgui_itemtooltip(toggle)) {
                        // int minx = std::max(celx - 20, 0);
                        // int miny = std::max(cely - 20, 0);
                        // int maxx = std::min(celx + 20, tile_size.width);
                        // int maxy = std::min(cely + 20, tile_size.height);

                        // TODO: simplify...
                        assert(tile_size.width >= 40);
                        assert(tile_size.height >= 40);
                        int minx = celx - 20, miny = cely - 20;
                        int maxx = celx + 20, maxy = cely + 20;
                        if (minx < 0) {
                            minx = 0, maxx = 40;
                        }
                        if (miny < 0) {
                            miny = 0, maxy = 40;
                        }
                        if (maxx > tile_size.width) {
                            minx = tile_size.width - 40, maxx = tile_size.width;
                        }
                        if (maxy > tile_size.height) {
                            miny = tile_size.height - 40, maxy = tile_size.height;
                        }
                        assert(maxx - minx == 40 && maxy - miny == 40);

                        const int w = maxx - minx, h = maxy - miny;
                        ImGui::PushTextWrapPos(200);
                        imgui_str("Right click to turn on/off the tooltip");
                        ImGui::PopTextWrapPos();
                        ImGui::Image(img.texture(), ImVec2(40 * 4, 40 * 4),
                                     {(float)minx / tile_size.width, (float)miny / tile_size.height},
                                     {(float)maxx / tile_size.width, (float)maxy / tile_size.height});
                    }
                }
            }

            // TODO: this shall belong to the runner.
            // TODO: precedence against left-clicking?
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                // ctrl.pause = true;
                const int celx = floor((mouse_pos.x - img_pos.x) / img_zoom);
                const int cely = floor((mouse_pos.y - img_pos.y) / img_zoom);

                sel.select_0.x = std::clamp(celx, 0, tile_size.width - 1);
                sel.select_0.y = std::clamp(cely, 0, tile_size.height - 1);
            }
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                const int celx = floor((mouse_pos.x - img_pos.x) / img_zoom);
                const int cely = floor((mouse_pos.y - img_pos.y) / img_zoom);

                sel.select_1.x = std::clamp(celx, 0, tile_size.width - 1);
                sel.select_1.y = std::clamp(cely, 0, tile_size.height - 1);
            }
            if (paste) {
                const int celx = floor((mouse_pos.x - img_pos.x) / img_zoom);
                const int cely = floor((mouse_pos.y - img_pos.y) / img_zoom);

                // TODO: can width<paste.width here?
                paste_beg.x = std::clamp(celx, 0, tile_size.width - paste->width());
                paste_beg.y = std::clamp(cely, 0, tile_size.height - paste->height());

                if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    legacy::copy<legacy::copyE::Or>(runner.tile(), paste_beg, *paste);
                    paste.reset();
                }
            }

            // TODO (temp) moved here to avoid affecting other utils (which rely on img_pos)
            if (imgui_scrolling()) {
                const ImVec2 cellidx = (mouse_pos - img_pos) / img_zoom;
                if (imgui_scrolldown() && img_zoom != 1) {
                    img_zoom /= 2;
                }
                if (imgui_scrollup() && img_zoom != 8) {
                    img_zoom *= 2;
                }
                img_off = (mouse_pos - cellidx * img_zoom) - canvas_pos;
                img_off.x = round(img_off.x);
                img_off.y = round(img_off.y); // TODO: is rounding correct?
            }
        }

        // TODO: "shrink selection" utility?
        if (imgui_keypressed(ImGuiKey_A, false)) {
            sel.toggle_select_all(tile_size);
        }
        if (imgui_keypressed(ImGuiKey_V, false)) {
            if (const char* text = ImGui::GetClipboardText()) {
                try {
                    // TODO: or ask whether to resize runner.tile?
                    paste.emplace(legacy::from_RLE_str(text, tile_size));
                } catch (const std::exception& err) {
                    logger::log_temp(2500ms, "{}", err.what());
                }
            }
        }
        if (const auto range = sel.range(); range.height() > 1 || range.width() > 1) {
            if (imgui_keypressed(ImGuiKey_C, false) || imgui_keypressed(ImGuiKey_X, false)) {
                ImGui::SetClipboardText(legacy::to_RLE_str(ctrl.rule, runner.tile(), range).c_str());
            }
            if (imgui_keypressed(ImGuiKey_Backspace, false) || imgui_keypressed(ImGuiKey_X, false)) {
                // TODO: 0/1.../ agar...
                legacy::clear_inside(runner.tile(), range, 0);
            }
            if (imgui_keypressed(ImGuiKey_Equal, false)) {
                // TODO: specify density etc... or share with tileT_filler?
                legacy::random_fill(runner.tile(), global_mt19937(), 0.5, range);
            }
            // TODO: redesign keyboard ctrl...
            if (imgui_keypressed(ImGuiKey_0, false)) {
                legacy::clear_outside(runner.tile(), range, 0);
            }

            // TODO: refine capturing...
            if (imgui_keypressed(ImGuiKey_P, false)) {
                // TODO: support specifying padding area...
                legacy::tileT cap(range.size()), cap2(range.size());
                legacy::copy(cap, {0, 0}, runner.tile(), range);
                locked = {};
                auto rulx = [&](legacy::codeT code) {
                    locked[code] = true;
                    return ctrl.rule[code];
                };
                // TODO: how to decide generation?
                for (int g = 0; g < 50; ++g) {
                    cap.gather(cap, cap, cap, cap, cap, cap, cap, cap);
                    cap.apply(rulx, cap2);
                    cap.swap(cap2);
                }
            }
        }
    };

    // TODO: not robust
    // ~ runner.restart(...) shall not happen before rendering.
    bool should_restart = false;
    int extra = 0;

    auto edit_ctrl = [&] {
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

            // TODO: visual feedback...
            if (ImGui::Button("+1")) {
                extra = 1;
            }
            ImGui::SameLine();
            // TODO: is this usage of ### correct?
            // (Correct, but usage of format might be a bad idea here...)
            if (ImGui::Button(std::format("+p({})###+p", ctrl.actual_pace()).c_str())) {
                extra = ctrl.actual_pace();
            }
            ImGui::PopButtonRepeat();
            ImGui::SameLine();
            if (ImGui::Button("Restart") || imgui_keypressed(ImGuiKey_R, false)) {
                should_restart = true;
            }

            // TODO: Gap-frame shall be really timer-based...
            ImGui::SliderInt("Gap Frame (0~20)", &ctrl.gap_frame, ctrl.gap_min, ctrl.gap_max, "%d",
                             ImGuiSliderFlags_NoInput);

#ifdef ENABLE_START_GEN
            imgui_int_slider("Start gen (0~200)", &ctrl.start_from, ctrl.start_min, ctrl.start_max);
#endif // ENABLE_START_GEN

            imgui_int_slider("Pace (1~20)", &ctrl.pace, ctrl.pace_min, ctrl.pace_max);
            ImGui::AlignTextToFramePadding();
            ImGui::Text("(Actual pace: %d)", ctrl.actual_pace());
            ImGui::SameLine();
            ImGui::Checkbox("anti-flick", &ctrl.anti_flick);
        }
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        {
            if (int seed = init.seed; imgui_int_slider("Seed (0~99)", &seed, 0, 99)) {
                init.seed = seed;
                should_restart = true;
            }

            // TODO: integer(ratio) density?
            if (ImGui::SliderFloat("Init density (0~1)", &init.density, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_NoInput)) {
                should_restart = true;
            }
        }
        ImGui::EndGroup();

        // TODO: enable/disable keyboard ctrl (enable by default)
        // TODO: redesign keyboard ctrl...
        if (imgui_keypressed(ImGuiKey_1, true)) {
            ctrl.gap_frame = std::max(ctrl.gap_min, ctrl.gap_frame - 1);
        }
        if (imgui_keypressed(ImGuiKey_2, true)) {
            ctrl.gap_frame = std::min(ctrl.gap_max, ctrl.gap_frame + 1);
        }
        if (imgui_keypressed(ImGuiKey_3, true)) {
            ctrl.pace = std::max(ctrl.pace_min, ctrl.pace - 1);
        }
        if (imgui_keypressed(ImGuiKey_4, true)) {
            ctrl.pace = std::min(ctrl.pace_max, ctrl.pace + 1);
        }
        // TODO: explain... apply to other ctrls?
        if ((ctrl.pause2 || !ImGui::GetIO().WantCaptureKeyboard) && ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
            ctrl.pause = !ctrl.pause;
        }
        // TODO: temp (this function turns out to be necessary...)
        if (imgui_keypressed(ImGuiKey_M, true)) {
            if (ctrl.pause) {
                extra = ctrl.actual_pace();
            }
            ctrl.pause = true;
        }
    };

    ImGui::PushItemWidth(FixedItemWidth);
    edit_ctrl();
    show_tile();
    ImGui::PopItemWidth();

    // TODO: (temp) this part was outside of begin_frame/end_frame...
    if (ctrl.rule != rule) {
        ctrl.rule = rule;
        should_restart = true;
        ctrl.pause = false; // TODO: this should be configurable...
    }
    if (should_restart) {
        runner.restart(init);
    }
    ctrl.run(runner, extra); // TODO: able to result in low fps...
}
