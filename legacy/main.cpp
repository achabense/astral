﻿// Dear ImGui: standalone example application for SDL2 + SDL_Renderer
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context
// creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// Important to understand: SDL_Renderer is an _optional_ component of SDL2.
// For a multi-platform app consider using e.g. SDL+DirectX on Windows and SDL+OpenGL on Linux/OSX.

#include <SDL.h>
#include <random>
#include <regex>
#include <vector>

#include "imgui.h"
#include "imgui_sdl2/imgui_impl_sdl2.h"
#include "imgui_sdl2/imgui_impl_sdlrenderer2.h"

#include "app.hpp"
#include "image.hpp"
#include "rule_traits.hpp"
#include "save.hpp"
#include "serialize2.hpp"

#if !SDL_VERSION_ATLEAST(2, 0, 17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

// TODO: awful... need to be avoided...
template <class Enum>
auto underlying_address(Enum& e) {
    return reinterpret_cast<std::underlying_type_t<Enum>*>(std::addressof(e));
}

// TODO: move into a header...
// TODO: forbid copying...
struct [[nodiscard]] imgui_itemtooltip {
    const bool opened; // TODO: proper name?
    imgui_itemtooltip() : opened(ImGui::BeginItemTooltip()) {}
    ~imgui_itemtooltip() {
        if (opened) {
            ImGui::EndTooltip();
        }
    }
    explicit operator bool() const { return opened; }
};

// TODO: clumsy...
// TODO: should be a class... how to decouple? ...
// TODO: for "paired", support 4-step modification (_,S,B,BS)... add new color?
void edit_rule(bool& show, const legacy::ruleT& to_edit, code_image& icons, rule_recorder& recorder) {
    using legacy::codeT;

    if (imgui_window window("Rule editor", &show, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
        window) {
        // TODO: are these info useful?
        // ImGui::Text("Spatial_symmtric:%d\tState_symmetric:%d\nABS_agnostic:%d\tXOR_agnostic:%d",
        //             legacy::spatial_symmetric(to_edit), legacy::state_symmetric(to_edit),
        //             legacy::center_agnostic_abs(to_edit), legacy::center_agnostic_xor(to_edit));

        ImGui::SeparatorText("Rule str");
        {
            auto rule_str = to_MAP_str(to_edit);
            ImGui::TextWrapped(rule_str.c_str());

            if (ImGui::Button("Copy to clipboard")) {
                ImGui::SetClipboardText(rule_str.c_str());
            }
            ImGui::SameLine();
            if (ImGui::Button("Save to file")) {
                legacy::record_rule(to_edit); // TODO: wasteful...
            }
        }

        static legacy::partition::basic_specification base = {};
        static legacy::partition::extra_specification extr = {};
        static legacy::interT inter = {};

        ImGui::SeparatorText("Settings");
        {
            ImGui::Combo("##MainMode", underlying_address(base), legacy::partition::basic_specification_names,
                         std::size(legacy::partition::basic_specification_names));

            // TODO: use extra_specification_names...
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Extr");
            ImGui::SameLine();
            ImGui::RadioButton("basic", underlying_address(extr), legacy::partition::extra_specification::none);
            ImGui::SameLine();
            ImGui::RadioButton("paired", underlying_address(extr), legacy::partition::paired);
            ImGui::SameLine();
            // TODO: explain...
            ImGui::RadioButton("state", underlying_address(extr), legacy::partition::state);

            ImGui::Text("Groups: %d", legacy::partition::get_partition(base, extr).k()); // TODO: use variable "part"?
            ImGui::Separator();

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Interp"); // TODO: suitable name...
            ImGui::SameLine();
            // TODO: might be better named "direct"
            ImGui::RadioButton("Val", underlying_address(inter.tag), inter.Value);
            ImGui::SameLine();
            ImGui::RadioButton("Flp", underlying_address(inter.tag), inter.Flip);
            ImGui::SameLine();
            ImGui::RadioButton("Dif", underlying_address(inter.tag), inter.Diff);
            if (inter.tag == inter.Diff) {
                ImGui::SameLine();
                if (ImGui::Button("Take current")) {
                    inter.custom = to_edit;
                }
                ImGui::TextWrapped(to_MAP_str(inter.custom).c_str());
            }
            // TODO: how to keep state-symmetry in Diff mode?
            ImGui::Text("State symmetry: %d", (int)legacy::state_symmetric(to_edit));
            ImGui::SameLine();
            if (ImGui::SmallButton("details")) {
                extr = legacy::partition::extra_specification::state;
                inter.tag = inter.Flip;
            }
        }

        const auto& part = legacy::partition::get_partition(base, extr);
        const int k = part.k();
        legacy::ruleT_data rule = inter.from_rule(to_edit);

        // TODO: should be foldable; should be able to set max height...
        ImGui::SeparatorText("Rule details");
        {
            {
                // TODO: unstable between base/extr switchs; ratio-based approach is on-trivial though... (double has
                // inaccessible values)
                static int rcount = 0.3 * k;
                rcount = std::clamp(rcount, 0, k);

                ImGui::SliderInt("##Active", &rcount, 0, k, "%d", ImGuiSliderFlags_NoInput);
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0)); // TODO: it this spacing suitable?
                ImGui::PushButtonRepeat(true);
                const float r = ImGui::GetFrameHeight();
                ImGui::SameLine();
                if (ImGui::Button("-", ImVec2(r, r))) {
                    rcount = std::max(0, rcount - 1);
                }
                ImGui::SameLine();
                if (ImGui::Button("+", ImVec2(r, r))) {
                    rcount = std::min(k, rcount + 1);
                }
                ImGui::PopButtonRepeat();
                ImGui::PopStyleVar();

                ImGui::SameLine();
                if (ImGui::Button("Randomize")) {
                    static std::mt19937_64 rand(time(0));
                    legacy::ruleT_data grule{};
                    random_fill(grule.data(), grule.data() + k, rcount, rand);
                    rule = legacy::partition::get_partition(base, extr).dispatch_from(grule);
                }
            }
            const auto& groups = part.groups();
            auto scans = part.scan(rule);
            {
                // TODO: expreimental; not suitable place... should be totally redesigned...
                ImGui::SameLine();
                if (ImGui::Button("???")) {
                    std::vector<legacy::compressT> vec;
                    vec.emplace_back(inter.to_rule(rule));
                    for (int j = 0; j < k; ++j) {
                        // TODO: whether to flip inconsistent units?
                        if (scans[j] != scans.Inconsistent) {
                            legacy::ruleT_data rule_j = rule;
                            for (codeT code : groups[j]) {
                                rule_j[code] = !rule_j[code];
                            }
                            vec.emplace_back(inter.to_rule(rule_j));
                        }
                    }
                    recorder.replace(vec); // TODO: awkward...
                    // TODO: relying on final take() having no effect...
                }
            }

            ImGui::Text("[1:%d] [0:%d] [x:%d]", scans.count(scans.All_1), scans.count(scans.All_0),
                        scans.count(scans.Inconsistent));

            ImGui::PushStyleVar(ImGuiStyleVar_::ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
            for (int j = 0; j < k; ++j) {
                if (j % 8 != 0) {
                    ImGui::SameLine();
                }

                const ImVec2 icon_size(3 * 7, 3 * 7); // zoom = 7.
                const bool inconsistent = scans[j] == scans.Inconsistent;

                ImGui::PushID(j);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
                if (inconsistent) {
                    // TODO: this looks super dumb...
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6, 0, 0, 1));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8, 0, 0, 1));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9, 0, 0, 1));
                }
                if (ImGui::ImageButton(icons.texture(), icon_size, ImVec2(0, groups[j][0] * (1.0f / 512)),
                                       ImVec2(1, (groups[j][0] + 1) * (1.0f / 512)))) {
                    // TODO: document this behavior... (keyctrl->resolve conflicts)
                    if (ImGui::GetIO().KeyCtrl) {
                        const bool b = !rule[groups[j][0]];
                        for (codeT code : groups[j]) {
                            rule[code] = b;
                        }
                    } else {
                        for (codeT code : groups[j]) {
                            rule[code] = !rule[code];
                        }
                    }

                    // TODO: TextUnformatted(strs[scans[j]]) may yield false result at this frame, but is negligible...
                }
                if (inconsistent) {
                    ImGui::PopStyleColor(3);
                }
                ImGui::PopStyleVar();
                ImGui::PopID();

                if (imgui_itemtooltip tooltip; tooltip) {
                    int x = 0;
                    for (codeT code : groups[j]) {
                        if (x++ % 8 != 0) {
                            ImGui::SameLine();
                        }

                        // TODO: use the same bordercol as button's?
                        // TODO: icons should provide codeT methods...
                        ImGui::Image(icons.texture(), icon_size, ImVec2(0, code * (1.0f / 512)),
                                     ImVec2(1, (code + 1) * (1.0f / 512)), ImVec4(1, 1, 1, 1),
                                     ImVec4(0.5, 0.5, 0.5, 1));
                        ImGui::SameLine();
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted(rule[code] ? ":1" : ":0");
                    }
                }

                ImGui::SameLine();
                ImGui::AlignTextToFramePadding();
                static const char* const strs[]{":x", ":0", ":1"};
                ImGui::TextUnformatted(strs[scans[j]]); // TODO: is ":x" useless?
            }
            ImGui::PopStyleVar();
        }
        recorder.take(inter.to_rule(rule));
    }
}

#if 0
// I have no idea whether I should use things like this...
const std::filesystem::path& get_pref_path() {
    static const std::filesystem::path pref = []() {
        std::unique_ptr<char, decltype(+SDL_free)> p(SDL_GetPrefPath("wtf", "wtfapp"), SDL_free);
        if (!p) {
            logger::log("{}", SDL_GetError());
            throw 0; // TODO: what if !p?
        }
        // TODO: p must be utf-8; on the other way, what encoding does fs::path assume?
        return std::filesystem::path(p.get());
    }();
    return pref;
}
#endif

// TODO: is "record" a returnable type?
// TODO: logic cleanup; must be raii... as exceptions can happen...
// TODO: add input...
// TODO: undo operation...
// TODO: class-ify?
[[nodiscard]] std::optional<std::vector<legacy::compressT>> file_nav(const char* id_str, bool* p_open = nullptr) {
    using namespace std;
    using namespace std::filesystem;

    // static path pos = current_path();
    static path pos = R"(C:\*redacted*\Desktop\rulelists_new)";
    // static path pos = get_pref_path();

    optional<path> try_open = nullopt;

    if (imgui_window window(id_str, p_open); window) {
        try {
            ImGui::TextUnformatted((const char*)pos.u8string().c_str());

            ImGui::Separator();
            // TODO: how to switch driver on windows? (enough to enable text input...)
            if (ImGui::MenuItem("Program current path")) {
                pos = current_path(); // TODO: what if invalid?
            }
            path par = pos.parent_path();
            if (ImGui::MenuItem("..", "dir", nullptr, par != pos && is_directory(par))) {
                pos = par;
            }
            ImGui::Separator();

            // TODO: what if too many entries?
            // TODO: move into child window; scope-guard required...
            int entries = 0;
            for (const auto& entry : directory_iterator(pos)) {
                auto str = entry.path().filename().u8string(); // TODO: is showing filename-only a good idea?
                // TODO: should ".txt" be needed? or should this be just be a default regex filter?
                bool txt = entry.is_regular_file() && entry.path().extension() == ".txt";
                bool dir = entry.is_directory();
                if (txt || dir) {
                    ++entries;
                    if (ImGui::MenuItem((const char*)str.c_str(), dir ? "dir" : "")) {
                        if (txt) {
                            // TODO: want double click; how?
                            try_open = entry.path();
                        } else if (dir) {
                            pos = entry.path();
                            break;
                        }
                    }
                }
            }
            if (entries == 0) {
                // TODO: better msg...
                ImGui::MenuItem("None##None", nullptr, nullptr, false);
            }
        } catch (const exception& what) {
            logger::log("Exception: {}", what.what()); // TODO: a lot of messy fs exceptions (access, encoding)...
            pos = pos.parent_path();                   // TODO: maybe fail again?
        }
    }

    if (try_open) {
        logger::log("Tried to open {}", try_open->filename().string());
        return read_rule_from_file(try_open->string().c_str());
    } else {
        return nullopt;
    }
}

tile_filler filler(/* seed= */ 0);
rule_runner runner({.width = 320, .height = 240});
rule_recorder recorder;

constexpr int pergen_min = 1, pergen_max = 20;
int pergen = 1;

constexpr int start_min = 0, start_max = 1000;
int start_from = 0;

constexpr int gap_min = 0, gap_max = 20;
int gap_frame = 0;

bool anti_flick = true; // TODO: add explanation

// TODO: should enable guide-mode (with a switch...)
// TODO: imgui_widgets_extension; see TextDisabled for details...
// TODO: changes upon the same rule should be grouped together... how? (editor++)...
// TODO: support seeding...(how to save?) some patterns are located in certain seed states...
// TODO: how to capture certain patterns? (editor++)...

// TODO: move the check elsewhere.
// C/C++ - command line - /utf-8
// and save file as utf8-encoding...
template <class T, class U>
constexpr bool bytes_equal(const T& t, const U& u) noexcept {
    if constexpr (sizeof(t) != sizeof(u)) {
        return false;
    } else {
        using A = std::array<std::byte, sizeof(t)>;
        return std::bit_cast<A>(t) == std::bit_cast<A>(u);
    }
}

static_assert(bytes_equal("中文", u8"中文"));

int main(int argc, char** argv) {
    logger::log("Entered main");

    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    // Create window with SDL_Renderer graphics context
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+SDL_Renderer example", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        SDL_Log("Error creating SDL_Renderer!");
        return 0;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // init:
    // recorder.take(legacy::game_of_life()); // first rule -> ctor
    if (argc == 2) {
        // TODO: add err logger
        recorder.replace(read_rule_from_file(argv[1]));
    }

    runner.set_rule(recorder.current());
    runner.restart(filler);

    // TODO: raii works terribly with C-style cleanup... can texture be destroyed after renderer?
    tile_image img(renderer, runner.tile()); // TODO: ...
    code_image icons(renderer);              // TODO: how to get "renderer" from backend?

    bool paused = false;
    bool show_rule_editor = true;
    bool show_demo_window = false; // TODO: remove this in the future...
    bool show_log_window = true;
    bool show_nav_window = true;

    auto actual_pergen = [](int pergen) {
        if (anti_flick) {
            if (legacy::will_flick(runner.rule()) && pergen % 2) {
                if (pergen == 1) {
                    ++pergen;
                } else {
                    assert(pergen >= 2);
                    --pergen;
                }
            }
        }
        return pergen;
    };

    // Main loop
    bool done = false;
    while (!done) {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your
        // inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or
        // clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or
        // clear/overwrite your copy of the keyboard data. Generally you may always pass all inputs to dear imgui, and
        // hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                done = true;
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window)) {
                done = true;
            }
        }

        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Frame state:
        // TODO: applying following logic; cosider refining it.
        // recorder is modified during display, but will synchronize with runner's before next frame.
        assert(runner.rule() == recorder.current());
        bool should_restart = false;

        // TODO: remove this when all done...
        if (show_demo_window) {
            ImGui::ShowDemoWindow(&show_demo_window);
        }

        if (show_log_window) {
            logger::window("Events", &show_log_window);
        }
        // TODO: editor works still poorly with recorder...
        if (show_rule_editor) {
            edit_rule(show_rule_editor, runner.rule(), icons, recorder);
        }
        if (show_nav_window) {
            // TODO: temporarily have to be here; works poorly with `recorder.take(edited);` logic...
            // TODO: as popup instead?
            auto result = file_nav("File Nav", &show_nav_window);
            // TODO: move logging to read_rule_from_file?
            if (result) {
                if (!result->empty()) {
                    logger::log("Found {} rules", result->size());
                    recorder.replace(std::move(*result));
                } else {
                    logger::log("Found nothing");
                }
            }
        }

        if (imgui_window window("-v-", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
            window) {
            ImGui::Checkbox("Log window", &show_log_window);
            ImGui::SameLine();
            ImGui::Checkbox("Rule editor", &show_rule_editor);
            ImGui::SameLine();
            ImGui::Checkbox("Nav window", &show_nav_window);

            ImGui::Checkbox("Demo window", &show_demo_window);

            ImGui::Text("(%.1f FPS) Frame:%d\n", io.Framerate, ImGui::GetFrameCount());
            ImGui::Separator();

            ImGui::Text("Width:%d,Height:%d,Density:%f", runner.tile().width(), runner.tile().height(),
                        float(runner.tile().count()) / runner.tile().area());
            {
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                const ImVec2 img_size = ImVec2(img.width(), img.height()); // TODO: support zooming?

                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
                ImGui::ImageButton(img.texture(), img_size);
                ImGui::PopStyleVar();

                // I did a trick here. ImageButton didn't draw eagerly, so it's safe to update the texture after
                // ImageButton(). This is used to provide stable view against dragging.
                // TODO: is this a good design?
                if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    runner.shift_xy(io.MouseDelta.x, io.MouseDelta.y);
                }
                img.update(runner.tile());

                if (imgui_itemtooltip tooltip; tooltip) {
                    // TODO: rewrite logic...
                    const float region_sz = 32.0f;
                    float region_x = io.MousePos.x - pos.x - region_sz * 0.5f;
                    float region_y = io.MousePos.y - pos.y - region_sz * 0.5f;
                    region_x = std::clamp(region_x, 0.0f, img_size.x - region_sz);
                    region_y = std::clamp(region_y, 0.0f, img_size.y - region_sz);

                    const ImVec2 uv0 = ImVec2((region_x) / img_size.x, (region_y) / img_size.y);
                    const ImVec2 uv1 = ImVec2((region_x + region_sz) / img_size.x, (region_y + region_sz) / img_size.y);
                    const float zoom = 4.0f; // TODO: should be settable?
                    ImGui::Image(img.texture(), ImVec2(region_sz * zoom, region_sz * zoom), uv0, uv1);

                    ImGui::TextUnformatted("Rclick to copy to clipboard.");
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_::ImGuiMouseButton_Right)) {
                        ImGui::SetClipboardText(to_MAP_str(runner.rule()).c_str());
                        logger::log("Copied rule with hash {}",
                                    0xffff & (std::hash<std::string>{}(to_MAP_str(runner.rule()))));
                        // TODO: should refine msg.
                        // TODO: should be shared with editor's.
                    }
                    ImGui::TextUnformatted("Ctrl+Lclick to paste from clipboard.");
                    if (io.KeyCtrl && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        const char* text = ImGui::GetClipboardText();
                        // TODO: can text return nullptr?
                        if (text) {
                            auto rules = extract_rules(text, text + strlen(text));
                            if (!rules.empty()) {
                                int size = recorder.size();
                                recorder.append(rules); // TODO: requires non-trivial append logic.
                                recorder.set_pos(size);
                                logger::log("Got {} rules from clipboard", rules.size());
                            } else {
                                logger::log("X_X");
                            }
                        }
                    }

                    if (io.MouseWheel < 0) { // scroll down
                        recorder.next();
                    } else if (io.MouseWheel > 0) { // scroll up
                        recorder.prev();
                    }
                    // TODO: recorder actions should ideally be centralized...
                }

                // TODO: again, can size()==0? NO, but how to guarantee?
                // It seems the whole recorder model is problematic...
                ImGui::AlignTextToFramePadding(); // TODO: +1 is clumsy.
                ImGui::Text("Total:%d At:%d", recorder.size(), recorder.pos() + 1);
                // TODO: pos may not reflect runner's real pos, as recorder can be modified on the way... may not
                // matters

                static char go_to[20]{};
                const auto filter = [](ImGuiInputTextCallbackData* data) {
                    return (data->EventChar >= '0' && data->EventChar <= '9') ? 0 : 1;
                };
                ImGui::SameLine();
                if (ImGui::InputTextWithHint(
                        "##Goto", "GOTO e.g. 2->enter", go_to, 20,
                        ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_EnterReturnsTrue, filter)) {
                    int val{};
                    if (sscanf(go_to, "%d", &val) == 1) {
                        recorder.set_pos(val - 1); // TODO: -1 is clumsy.
                    }
                    go_to[0] = '\0';

                    // Regain focus:
                    ImGui::SetKeyboardFocusHere(-1);
                }
            }

            ImGui::SeparatorText("Tile");
            {
                ImGui::Text("Gen:%d", runner.gen());

                // TODO: this is horribly obscure...
                if (ImGui::SliderFloat("Init density [0-1]", &filler.density, 0.0f, 1.0f, "%.3f",
                                       ImGuiSliderFlags_NoInput)) {
                    should_restart = true;
                }

                ImGui::Checkbox("Pause", &paused);
                if (ImGui::IsKeyPressed(ImGuiKey_P, false)) {
                    paused = !paused;
                }

                ImGui::SameLine();
                ImGui::PushButtonRepeat(true); // accept consecutive clicks... TODO: too fast...
                if (ImGui::Button("+1")) {
                    runner.run(1); // TODO: should run be called here?
                }
                ImGui::SameLine();
                if (ImGui::Button("+p")) {
                    runner.run(actual_pergen(pergen)); // TODO: should combine...
                }
                ImGui::PopButtonRepeat();

                ImGui::SameLine();
                // TODO: conflicts with text input. should have scope.
                if (ImGui::Button("Restart") || ImGui::IsKeyPressed(ImGuiKey_R, false)) {
                    should_restart = true;
                }
                ImGui::SameLine();
                // TODO: conflicts with text input. should have scope.
                if (ImGui::Button("Reseed") || ImGui::IsKeyPressed(ImGuiKey_S, false)) {
                    filler.disturb();
                    should_restart = true;
                }
            }

            {
                // TODO: toooo ugly...
                // TODO: (?) currently suitable to restart immediately...
                ImGui::SliderInt("Per gen [1-20]", &pergen, pergen_min, pergen_max, "%d", ImGuiSliderFlags_NoInput);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("(Actual: %d)", actual_pergen(pergen));
                ImGui::SameLine();
                ImGui::Checkbox("anti-flick", &anti_flick);

                ImGui::SliderInt("Gap Frame [0-20]", &gap_frame, gap_min, gap_max, "%d", ImGuiSliderFlags_NoInput);
                ImGui::SliderInt("Start gen [0-1000]", &start_from, start_min, start_max, "%d",
                                 ImGuiSliderFlags_NoInput);

                // TODO: conflicts with text input. should have scope.
                // TODO: which(_1, _2) should be used for gap_frame+=1?
                if (ImGui::IsKeyPressed(ImGuiKey_1, true)) {
                    gap_frame = std::max(gap_min, gap_frame - 1);
                }
                if (ImGui::IsKeyPressed(ImGuiKey_2, true)) {
                    gap_frame = std::min(gap_max, gap_frame + 1);
                }
                if (ImGui::IsKeyPressed(ImGuiKey_3, true)) {
                    pergen = std::max(pergen_min, pergen - 1);
                }
                if (ImGui::IsKeyPressed(ImGuiKey_4, true)) {
                    pergen = std::min(pergen_max, pergen + 1);
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Space, true)) {
                    if (!paused) {
                        paused = true;
                    } else {
                        runner.run(actual_pergen(pergen)); // ?run(1)
                    }
                }
            }
        }

        // Rendering
        ImGui::Render();
        SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255),
                               (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(renderer);

        // Synchronize with recorder:
        should_restart |= runner.set_rule(recorder.current()); // is || (instead of | here) app-logically safe?
        if (should_restart) {
            runner.restart(filler);
        }
        if (runner.gen() < start_from) {
            runner.run(start_from - runner.gen());
        } else if (!paused) {
            // TODO: is GetFrameCount unchanged since last call in tile window?
            if (ImGui::GetFrameCount() % (gap_frame + 1) == 0) {
                runner.run(actual_pergen(pergen));
            }
        }
    }

    // TODO: should destroy img and icons here... (depending on where they defined now)

    // Cleanup
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// TODO: whether to accept <imgui.h> as base header?

namespace legacy {
    void run_and_swap(tileT& major, tileT& helper, const ruleT& rule) {
        assert(&major != &helper);
        major.gather().apply(rule, helper);
        major.swap(helper);
    }

    void shift_xy(tileT& major, tileT& helper, int dx, int dy) {
        const int width = major.width(), height = major.height();

        dx = ((-dx % width) + width) % width;
        dy = ((-dy % height) + height) % height;
        if (dx == 0 && dy == 0) {
            return;
        }

        helper.resize(major.size());
        for (int y = 0; y < height; ++y) {
            bool* source = major.line((y + dy) % height);
            bool* dest = major.line(y);
            std::copy_n(source, width, dest);
            std::rotate(dest, dest + dx, dest + width);
        }
        major.swap(helper);
    }

} // namespace legacy

////////////////////////////////////////////////////////////////////////////////
