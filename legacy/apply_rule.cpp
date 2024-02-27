#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "tile.hpp"

#include "common.hpp"

static void refresh(tile_image& img, const legacy::tileT& tile) {
    img.refresh(tile.width(), tile.height(), [&tile](int y) { return tile.line(y); });
}

static void run_torus(legacy::tileT& tile, legacy::tileT& temp, const legacy::rule_like auto& rule) {
    assert(&tile != &temp);

    tile.gather(tile, tile, tile, tile, tile, tile, tile, tile);
    tile.apply(rule, temp);
    tile.swap(temp);
}

// Copy the subrange and run as a torus space, recording all invoked mappings.
// This is good at capturing "self-contained" patterns (oscillators/spaceships).
static legacy::moldT::lockT capture_closed(const legacy::tileT& source, const legacy::tileT::rangeT& range,
                                           const legacy::ruleT& rule) {
    legacy::tileT tile(range.size());
    legacy::copy(tile, {0, 0}, source, range);

    legacy::tileT temp(range.size());

    legacy::moldT::lockT lock{};

    // (wontfix) It's possible that the loop fails to catch all invocations in very rare cases,
    // due to that `limit` is not large enough.

    // Loop until there has been `limit` generations without newly invoked mappings.
    const int limit = 30;
    for (int g = limit; g > 0; --g) {
        run_torus(tile, temp, [&](legacy::codeT code) {
            if (!lock[code]) {
                g = limit;
                lock[code] = true;
            }
            return rule[code];
        });
    }

    return lock;
}

// TODO: how to support open-capture in the gui?
#if 0
// However, if the patterns is not self-contained, what happens in the copied subrange (treated as torus space)
// is not exactly what we see in the source space.
// Especially, for the rules with interesting "texture"s (but no typical oscillators etc), `capture_closed` tends to
// make a full lock, which is not useful.
// ~ We may want to record what actually happened in the range (instead of the the captured pattern) during the run of
// the source space:
static void capture_open(const legacy::tileT& source, legacy::tileT::rangeT range, legacy::moldT::lockT& lock) {
    if (range.width() <= 2 || range.height() <= 2) {
        return;
    }
    range.begin.x += 1;
    range.begin.y += 1;
    range.end.x -= 1;
    range.end.y -= 1;
    source.record(lock, range);
}
#endif

class torusT {
    legacy::tileT m_tile, m_temp;
    int m_gen;

public:
    // Using float (instead of double) as there is only ImGui::SliderFloat.
    struct initT {
        legacy::tileT::sizeT size;
        uint32_t seed;
        float density; // ∈ [0.0f, 1.0f]
    };

    explicit torusT(const initT& init) : m_tile(init.size), m_temp(init.size), m_gen(0) { restart(init); }

    legacy::tileT& tile() { return m_tile; }
    const legacy::tileT& tile() const { return m_tile; }
    int gen() const { return m_gen; }

    void restart(const initT& init) {
        m_tile.resize(init.size);
        m_temp.resize(init.size);

        std::mt19937 rand(init.seed);
        legacy::random_fill(m_tile, rand, init.density);

        m_gen = 0;
    }

    void run(const legacy::ruleT& rule, int count = 1) {
        for (int c = 0; c < count; ++c) {
            run_torus(m_tile, m_temp, rule);
            ++m_gen;
        }
    }

    // TODO: recheck logic...
    void shift(int dx, int dy) {
        const int width = m_tile.width(), height = m_tile.height();

        const auto wrap = [](int v, int r) { return ((v % r) + r) % r; };
        dx = wrap(-dx, width);
        dy = wrap(-dy, height);
        if (dx == 0 && dy == 0) {
            return;
        }

        m_temp.resize(m_tile.size());
        for (int y = 0; y < height; ++y) {
            const bool* source = m_tile.line((y + dy) % height);
            bool* dest = m_temp.line(y);
            std::copy_n(source, width, dest);
            std::rotate(dest, dest + dx, dest + width);
        }
        m_tile.swap(m_temp);
    }
};

// TODO: rename...
// TODO: Document that this is not the only situation that flicking effect can occur...
static bool will_flick(const legacy::ruleT& rule) {
    constexpr legacy::codeT all_0{0}, all_1{511};
    return rule[all_0] == 1 && rule[all_1] == 0;
}

struct ctrlT {
    legacy::ruleT rule{};

    // TODO: better name?
    static constexpr int pace_min = 1, pace_max = 20;
    int pace = 1;
    bool anti_flick = true; // TODO: add explanation
    int actual_pace() const {
        if (anti_flick && will_flick(rule) && pace % 2) {
            return pace + 1;
        }
        return pace;
    }

    // TODO: redesign?
    static constexpr int gap_min = 0, gap_max = 20;
    int gap_frame = 0;

    bool pause = false;
    bool pause2 = false; // TODO: explain...

    void run(torusT& runner, int extra = 0) const {
        if (extra != 0) {
            runner.run(rule, extra);
        }
        if (!pause && !pause2) {
            if (ImGui::GetFrameCount() % (gap_frame + 1) == 0) {
                runner.run(rule, actual_pace());
            }
        }
    }
};

// TODO: support 1*1 selection?
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

std::optional<legacy::moldT::lockT> apply_rule(const legacy::ruleT& rule, tile_image& img) {
    std::optional<legacy::moldT::lockT> out = std::nullopt;

    // TODO: the constraint is arbitrary; are there more sensible ways to decide size constraint?
    // TODO: (temp) being related to min canvas size...
    const auto size_clamped = [](int width, int height) {
        return legacy::tileT::sizeT{.width = std::clamp(width, 20, 1200), .height = std::clamp(height, 10, 1200)};
    };

    static torusT::initT init{.size{.width = 500, .height = 400}, .seed = 0, .density = 0.5};
    static torusT runner(init);
    assert(init.size == runner.tile().size());
    assert(init.size == size_clamped(init.size.width, init.size.height));

    static ctrlT ctrl{.rule = rule, .pace = 1, .anti_flick = true, .gap_frame = 0, .pause = false};

    static ImVec2 img_off = {0, 0};
    static int img_zoom = 1;
    assert(img_off.x == int(img_off.x) && img_off.y == int(img_off.y));

    static std::optional<legacy::tileT> paste;
    static legacy::tileT::posT paste_beg{0, 0}; // dbegin for copy... (TODO: this is confusing...)

    static selectT sel{};

    // TODO: not robust
    // ~ runner.restart(...) shall not happen before rendering.
    bool should_restart = false;
    int extra = 0;

    if (ctrl.rule != rule) {
        ctrl.rule = rule;
        should_restart = true;
        ctrl.anti_flick = true;
        ctrl.pause = false;
    }

    static ImVec2 last_known_canvas_size{100, 100}; // TODO: make std::optional?

    // TODO: better be controlled by frame()?
    ImGui::PushItemWidth(item_width);

    // TODO: redesign keyboard ctrl...
    if (imgui_KeyPressed(ImGuiKey_1, true)) {
        ctrl.gap_frame = std::max(ctrl.gap_min, ctrl.gap_frame - 1);
    }
    if (imgui_KeyPressed(ImGuiKey_2, true)) {
        ctrl.gap_frame = std::min(ctrl.gap_max, ctrl.gap_frame + 1);
    }
    if (imgui_KeyPressed(ImGuiKey_3, true)) {
        ctrl.pace = std::max(ctrl.pace_min, ctrl.pace - 1);
    }
    if (imgui_KeyPressed(ImGuiKey_4, true)) {
        ctrl.pace = std::min(ctrl.pace_max, ctrl.pace + 1);
    }
    // TODO: explain... apply to other ctrls?
    if ((ctrl.pause2 || !ImGui::GetIO().WantCaptureKeyboard) && ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
        ctrl.pause = !ctrl.pause;
    }
    // Run by keystroke turns out to be necessary. (TODO: For example ...)
    if (imgui_KeyPressed(ImGuiKey_M, true)) {
        if (ctrl.pause) {
            extra = ctrl.actual_pace();
        }
        ctrl.pause = true;
    }

    ImGui::BeginGroup();
    {
        ImGui::Checkbox("Pause", &ctrl.pause);
        ImGui::SameLine();
        ImGui::BeginDisabled();
        ImGui::Checkbox("Pause2", &ctrl.pause2);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::PushButtonRepeat(true);
        if (ImGui::Button("+1")) {
            extra = 1;
        }
        if (ctrl.pause) {
            ImGui::SameLine();
            // TODO: The usage of format looks wasteful...
            if (ImGui::Button(std::format("+p({})###+p", ctrl.actual_pace()).c_str())) {
                extra = ctrl.actual_pace();
            }
        }
        ImGui::PopButtonRepeat();
        ImGui::SameLine();
        if (ImGui::Button("Restart") || imgui_KeyPressed(ImGuiKey_R, false)) {
            should_restart = true;
        }

        // TODO: Gap-frame shall be really timer-based...
        imgui_StepSliderInt("Gap Frame (0~20)", &ctrl.gap_frame, ctrl.gap_min, ctrl.gap_max);

        imgui_StepSliderInt("Pace (1~20)", &ctrl.pace, ctrl.pace_min, ctrl.pace_max);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("(Actual pace: %d)", ctrl.actual_pace());
        ImGui::SameLine();
        ImGui::Checkbox("anti-flick", &ctrl.anti_flick);
    }
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginGroup();
    {
        if (int seed = init.seed; imgui_StepSliderInt("Init seed (0~99)", &seed, 0, 99)) {
            init.seed = seed;
            should_restart = true;
        }

        if (ImGui::SliderFloat("Init density (0~1)", &init.density, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_NoInput)) {
            should_restart = true;
        }

        {
            static char input_width[20]{}, input_height[20]{};
            const auto filter = [](ImGuiInputTextCallbackData* data) -> int {
                return (data->EventChar >= '0' && data->EventChar <= '9') ? 0 : 1;
            };
            const float s = imgui_ItemInnerSpacingX();
            const float w = (ImGui::CalcItemWidth() - s * 2 - ImGui::CalcTextSize("Resize").x -
                             ImGui::GetStyle().FramePadding.x * 2) /
                            2; // TODO: is floor/ceil needed?
            ImGui::SetNextItemWidth(w);
            ImGui::InputTextWithHint("##Width", "width", input_width, std::size(input_width),
                                     ImGuiInputTextFlags_CallbackCharFilter, filter);
            ImGui::SameLine(0, s);
            ImGui::SetNextItemWidth(w);
            ImGui::InputTextWithHint("##Height", "height", input_height, std::size(input_height),
                                     ImGuiInputTextFlags_CallbackCharFilter, filter);
            ImGui::SameLine(0, s);
            if (ImGui::Button("Resize")) {
                int width = init.size.width, height = init.size.height;
                const bool has_w = std::from_chars(input_width, std::end(input_width), width).ec == std::errc{};
                const bool has_h = std::from_chars(input_height, std::end(input_height), height).ec == std::errc{};
                // ~ the value is unmodified if `from_chars` fails.
                if (has_w || has_h) {
                    img_off = {0, 0};
                    img_zoom = 1;
                    const legacy::tileT::sizeT size = size_clamped(width, height);
                    if (init.size != size) {
                        init.size = size;
                        runner.restart(init); // TODO: about vs setting should_restart...
                    }
                }
                input_width[0] = '\0';
                input_height[0] = '\0';
            }

            ImGui::SameLine(0, s);
            ImGui::Text("Width:%d, Height:%d", runner.tile().width(), runner.tile().height());
        }

        ImGui::AlignTextToFramePadding();
        imgui_Str("Fit with zoom =");
        ImGui::SameLine();
        for (const ImVec2 size = square_size(); int z : {1, 2, 4, 8}) {
            if (z != 1) {
                ImGui::SameLine(0, imgui_ItemInnerSpacingX());
            }
            if (ImGui::Button(std::to_string(z).c_str(), size)) {
                img_zoom = z;
                img_off = {0, 0};

                // TODO: explain that `last_known_canvas_size` works well...
                const legacy::tileT::sizeT size =
                    size_clamped((int)last_known_canvas_size.x / img_zoom, (int)last_known_canvas_size.y / img_zoom);
                if (init.size != size) {
                    init.size = size;
                    runner.restart(init);
                }
            }
        }
    }
    ImGui::EndGroup();

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
    const bool corner = ImGui::Button("Corner"); // TODO: move elsewhere...
    ImGui::SameLine();
    const bool center = ImGui::Button("Center");
    // TODO: select all, unselect button...

    if (paste) {
        ImGui::SameLine(), imgui_Str("|"), ImGui::SameLine();
        if (ImGui::Button("Drop paste")) {
            paste.reset();
        }
    }

    // TODO: move elsewhere in the gui?
    ImGui::Text("Gen:%d, Density:%.4f", runner.gen(), float(legacy::count(runner.tile())) / runner.tile().area());
    // TODO: canvas size, tile size, selected size, paste size...

    {
        ImGui::InvisibleButton("Canvas", [] {
            // Values of GetContentRegionAvail() can be negative...
            ImVec2 size = ImGui::GetContentRegionAvail();
            // TODO (temp) ~ min tile size * max zoom...
            return ImVec2(std::max(size.x, 160.0f), std::max(size.y, 80.0f));
        }());
        const ImVec2 canvas_min = ImGui::GetItemRectMin();
        const ImVec2 canvas_max = ImGui::GetItemRectMax();
        const ImVec2 canvas_size = ImGui::GetItemRectSize();
        last_known_canvas_size = canvas_size;

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
            // TODO (temp) /img_zoom->floor->*img_zoom, so that img_off will be {0, 0} after fitting.
            img_off.x = floor(img_off.x / img_zoom) * img_zoom;
            img_off.y = floor(img_off.y / img_zoom) * img_zoom;
        }

        const ImVec2 img_min = canvas_min + img_off;
        const ImVec2 img_max = img_min + img_size;

        ImDrawList* const drawlist = ImGui::GetWindowDrawList();
        drawlist->PushClipRect(canvas_min, canvas_max);
        drawlist->AddRectFilled(canvas_min, canvas_max, IM_COL32(20, 20, 20, 255));

        if (!paste) {
            refresh(img, runner.tile());

            drawlist->AddImage(img.texture(), img_min, img_max);
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
            refresh(img, runner.tile());
            legacy::copy(runner.tile(), paste_beg, temp);

            drawlist->AddImage(img.texture(), img_min, img_max);
            const ImVec2 paste_min = img_min + ImVec2(range.begin.x, range.begin.y) * img_zoom;
            const ImVec2 paste_max = img_min + ImVec2(range.end.x, range.end.y) * img_zoom;
            drawlist->AddRectFilled(paste_min, paste_max, IM_COL32(255, 0, 0, 60));
        }

        if (const auto range = sel.range(); range.width() > 1 || range.height() > 1) {
            const ImVec2 sel_min = img_min + ImVec2(range.begin.x, range.begin.y) * img_zoom;
            const ImVec2 sel_max = img_min + ImVec2(range.end.x, range.end.y) * img_zoom;
            drawlist->AddRectFilled(sel_min, sel_max, IM_COL32(0, 255, 0, 40));
            drawlist->AddRect(sel_min, sel_max, IM_COL32(0, 255, 0, 160));
        }
        drawlist->PopClipRect();

        const bool active = ImGui::IsItemActive();
        ctrl.pause2 = paste || active; // TODO: won't work if |= active... should ctrl.run clear pause2?
        if (ImGui::IsItemHovered()) {
            // TODO: reorganize...
            const ImGuiIO& io = ImGui::GetIO();

            assert(ImGui::IsMousePosValid());
            // It turned out that, this will work well even if outside of the image...
            const ImVec2 mouse_pos = io.MousePos;
            const ImVec2 cell_pos_raw = (mouse_pos - img_min) / img_zoom;
            const int celx = floor(cell_pos_raw.x);
            const int cely = floor(cell_pos_raw.y);

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
                if (celx >= -10 && celx < tile_size.width + 10 && cely >= -10 && cely < tile_size.height + 10) {
                    if (static bool toggle = true; auto tooltip = imgui_ItemTooltip(toggle)) {
                        const int w = std::min(tile_size.width, 40);
                        const int h = std::min(tile_size.height, 40);

                        int minx = celx - w / 2, miny = cely - h / 2;
                        int maxx = celx + (w - w / 2), maxy = cely + (h - h / 2);
                        if (minx < 0) {
                            minx = 0, maxx = w;
                        }
                        if (miny < 0) {
                            miny = 0, maxy = h;
                        }
                        if (maxx > tile_size.width) {
                            minx = tile_size.width - w, maxx = tile_size.width;
                        }
                        if (maxy > tile_size.height) {
                            miny = tile_size.height - h, maxy = tile_size.height;
                        }

                        assert(w == maxx - minx && h == maxy - miny);
                        // TODO: show some text...
                        ImGui::Image(img.texture(), ImVec2(w * 4, h * 4),
                                     {(float)minx / tile_size.width, (float)miny / tile_size.height},
                                     {(float)maxx / tile_size.width, (float)maxy / tile_size.height});
                    }
                }
            }

            // TODO: what if clicked from outside into the canvas?
            // TODO: precedence against left-clicking?
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                sel.select_0.x = std::clamp(celx, 0, tile_size.width - 1);
                sel.select_0.y = std::clamp(cely, 0, tile_size.height - 1);
            }
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                ctrl.pause2 = true; // TODO: works poorly with double-rclick toggling...
                sel.select_1.x = std::clamp(celx, 0, tile_size.width - 1);
                sel.select_1.y = std::clamp(cely, 0, tile_size.height - 1);
            }

            // TODO: refactor away this block...
            // TODO: the logic will be simplified if invisible button goes before the image...
            if (paste) {
                assert(paste->width() <= tile_size.width && paste->height() <= tile_size.height);
                paste_beg.x = std::clamp(celx, 0, tile_size.width - paste->width());
                paste_beg.y = std::clamp(cely, 0, tile_size.height - paste->height());

                if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    legacy::copy<legacy::copyE::Or>(runner.tile(), paste_beg, *paste);
                    paste.reset();
                }
            }

            if (imgui_MouseScrolling()) {
                if (imgui_MouseScrollingDown() && img_zoom != 1) {
                    img_zoom /= 2;
                }
                if (imgui_MouseScrollingUp() && img_zoom != 8) {
                    img_zoom *= 2;
                }
                img_off = (mouse_pos - cell_pos_raw * img_zoom) - canvas_min;
                img_off.x = round(img_off.x);
                img_off.y = round(img_off.y); // TODO: is rounding correct?
            }
        }

        if (imgui_KeyPressed(ImGuiKey_A, false)) {
            sel.toggle_select_all(tile_size);
        }
        if (imgui_KeyPressed(ImGuiKey_V, false)) {
            if (const char* text = ImGui::GetClipboardText()) {
                try {
                    // TODO: or ask whether to resize runner.tile?
                    paste.emplace(legacy::from_RLE_str(text, tile_size));
                } catch (const std::exception& err) {
                    messenger::add_msg("{}", err.what());
                }
            }
        }
        if (const auto range = sel.range(); range.height() > 1 || range.width() > 1) {
            // TODO: what if the right mouse is still pressed?
            if (imgui_KeyPressed(ImGuiKey_S, false)) {
                const auto [begin, end] = legacy::bounding_box(runner.tile(), range);
                if (begin != end) {
                    sel.select_0 = begin;
                    sel.select_1 = {.x = end.x - 1, .y = end.y - 1};
                } else {
                    sel.clear();
                }
            }
            if (imgui_KeyPressed(ImGuiKey_C, false) || imgui_KeyPressed(ImGuiKey_X, false)) {
                ImGui::SetClipboardText(legacy::to_RLE_str(ctrl.rule, runner.tile(), range).c_str());
            }
            if (imgui_KeyPressed(ImGuiKey_Backspace, false) || imgui_KeyPressed(ImGuiKey_X, false)) {
                legacy::clear_inside(runner.tile(), range);
            }
            if (imgui_KeyPressed(ImGuiKey_Equal, false)) {
                // TODO: specify density etc...
                legacy::random_fill(runner.tile(), global_mt19937(), 0.5, range);
            }
            // TODO: redesign keyboard ctrl...
            if (imgui_KeyPressed(ImGuiKey_0, false)) {
                legacy::clear_outside(runner.tile(), range);
            }

            if (imgui_KeyPressed(ImGuiKey_P, false)) {
                out = capture_closed(runner.tile(), range, ctrl.rule);
            }

            // TODO: support a menu somewhere...
            // (context menu works poorly with rbutton selection...)
#if 0
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Clear selected", "del")) { // 0/1
                }
                if (ImGui::MenuItem("Clear outside", "0")) { // 0/1
                }
                // randomize-density...
                if (ImGui::MenuItem("Randomize", "+")) {
                }
                if (ImGui::MenuItem("Copy", "c")) {
                }
                if (ImGui::MenuItem("Capture", "p")) {
                }
                ImGui::EndPopup();
            }
#endif
        }
    }

    ImGui::PopItemWidth();

    if (should_restart) {
        runner.restart(init);
    }
    ctrl.run(runner, extra); // TODO: able to result in low fps...

    return out;
}
