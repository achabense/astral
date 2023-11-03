// Dear ImGui: standalone example application for SDL2 + SDL_Renderer
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

rule_maker maker(/* seed= */ time(nullptr));
tile_filler filler(/* seed= */ 0);
rule_runner runner({.width = 320, .height = 240});
rule_recorder recorder;

// TODO: how to generalize this?
constexpr int pergen_min = 1, pergen_max = 20;
int pergen = 1;

// TODO: when is this needed?
constexpr int start_min = 0, start_max = 1000;
int start_from = 0;

// TODO: support pace formally...
// ~paceless, take another thread... (need locks)
// enum pace_mode { per_frame, per_duration };
constexpr int skip_min = 0, skip_max = 20;
int skip_per_frame = 0;

bool cal_rate = true;
bool anti_flick = true; // TODO: make settable...

// TODO: looks horrible and inefficient
std::string wrap_rule_string(const std::string& str) {
    return str.substr(0, 32) + "\n" + str.substr(32, 16) + "...";
}

// TODO: awful... need to be avoided...
template <class Enum> auto* underlying_address(Enum& ptr) {
    return reinterpret_cast<std::underlying_type_t<Enum>*>(std::addressof(ptr));
}

// TODO: clumsy...
// TODO: how to get renderer from backend?
// TODO: should be a class...
legacy::ruleT edit_rule(bool& show, const legacy::ruleT& old_rule, code_image& icons) {
    assert(show);
    if (!ImGui::Begin("Rule editor", &show,
                      ImGuiWindowFlags_::ImGuiWindowFlags_NoCollapse |
                          ImGuiWindowFlags_::ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return old_rule;
    }

    auto rule_str = to_MAP_str(old_rule);              // how to reuse the resource?
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 18); // TODO: "fontsize" is height
    ImGui::TextUnformatted("Rule str:");
    // TODO: better visual; use editor::member instead...
    ImGui::TextUnformatted(rule_str.c_str());
    ImGui::PopTextWrapPos();

    if (ImGui::Button("Copy to clipboard")) {
        ImGui::SetClipboardText(rule_str.c_str());
    }
    if (ImGui::Button("Save")) {
        legacy::record_rule(old_rule); // TODO: expreimental... use rule_str instead?
    }

    // TODO: are these info useful?
    ImGui::Text("Spatial symmtric:%d\nState_symmetric:%d\nABS_agnostic:%d XOR_agnostic:%d",
                legacy::spatial_symmetric(old_rule), legacy::state_symmetric(old_rule),
                legacy::center_agnostic_abs(old_rule), legacy::center_agnostic_xor(old_rule));

    static legacy::partition::extra_specification extr = {};

    // TODO: combine with rule_maker's
    // TODO: use extra_specification_names...
    ImGui::RadioButton("basic", underlying_address(extr), legacy::partition::extra_specification::none);
    ImGui::SameLine();
    ImGui::RadioButton("paired", underlying_address(extr), legacy::partition::paired);
    ImGui::SameLine();
    ImGui::RadioButton("state", underlying_address(extr), legacy::partition::state);

    static bool as_flip = false; // TODO: for as_flip, use characters other than "0" "1"
    ImGui::Checkbox("As-Flip", &as_flip);

    legacy::ruleT_data rule =
        legacy::from_rule(old_rule, as_flip ? legacy::interpret_mode::XOR : legacy::interpret_mode::ABS);

    // TODO: How to specify the first "BeginTabItem"?
    // ???ImGuiTabItemFlags_SetSelected
    // TODO: when will it return false?
    if (ImGui::BeginTabBar("##Type")) {
        for (int i = 0; i < legacy::partition::basic_specification::size; ++i) {
            // TODO: how to get/set current item?
            if (ImGui::BeginTabItem(legacy::partition::basic_specification_names[i])) {
            const auto& part = legacy::partition::get_partition(legacy::partition::basic_specification(i), extr);
            const auto& groups = part.groups();

                const int k = part.k();
                auto scans = part.scan(rule);

                ImGui::PushStyleVar(ImGuiStyleVar_::ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
                // buttons. TODO: better visual; show group members.
                for (int j = 0; j < k; ++j) {
                    if (j % 8 != 0) {
                        ImGui::SameLine();
                    }

                    // TODO: should be able to resolve conflicts...
                    if (scans[j] == legacy::scanT::inconsistent) {
                        ImGui::BeginDisabled();
                        // TODO: better visual...
                        ImGui::PushStyleColor(ImGuiCol_::ImGuiCol_Button, ImVec4(0.8, 0, 0, 1));
                    }
                    ImGui::PushID(j);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
                    if (ImGui::ImageButton(icons.texture(), ImVec2(3 * 8, 3 * 8),
                                           ImVec2(0, groups[j][0] * (1.0f / 512)),
                                           ImVec2(1, (groups[j][0] + 1) * (1.0f / 512)))) {
                        for (auto code : groups[j]) {
                            rule[code] = !rule[code];
                        }
                    }
                    ImGui::PopStyleVar();
                    ImGui::PopID();
                    if (scans[j] == legacy::scanT::inconsistent) {
                        ImGui::PopStyleColor();
                        ImGui::EndDisabled();
                    }
                    if (ImGui::BeginItemTooltip()) {
                        int x = 0;
                        for (int code : groups[j]) {
                            if (x++ % 8 != 0) {
                                ImGui::SameLine();
                            }

                            constexpr int zoom = 8;
                            // for bordercol...
                            ImGui::Image(icons.texture(), ImVec2(3 * zoom, 3 * zoom), ImVec2(0, code * (1.0f / 512)),
                                         ImVec2(1, (code + 1) * (1.0f / 512)), ImVec4(1, 1, 1, 1),
                                         ImVec4(0.5, 0.5, 0.5, 1));
                            if (scans[j] == legacy::scanT::inconsistent) {
                                ImGui::SameLine();
                                ImGui::AlignTextToFramePadding();
                                ImGui::TextUnformatted(rule[code] ? ":1" : ":0");
                            }
                        }
                        ImGui::EndTooltip();
                    }
                    ImGui::SameLine();
                    ImGui::AlignTextToFramePadding();
                    static const char* strs[]{":x", ":0", ":1"};
                    ImGui::TextUnformatted(strs[scans[j]]);
                }
                ImGui::PopStyleVar();

                ImGui::EndTabItem();
            }
        }

        ImGui::EndTabBar();
    }
    ImGui::End();
    return to_rule(rule, as_flip ? legacy::interpret_mode::XOR : legacy::interpret_mode::ABS);
}

// TODO: should enable guide-mode (with a switch...)
// TODO: imgui_widgets_extension; see TextDisabled for details...
// TODO: changes upon the same rule should be grouped together... how? (editor++)...
// TODO: support seeding...(how to save?) some patterns are located in certain seed states...
// TODO: how to capture certain patterns? (editor++)...

int main(int argc, char** argv) {
    // TODO: should be able to "open" a rulelist file...
    // legacy::convert_all_files();

    // TODO: what's the effect in "/SUBSYSTEM:WINDOWS" release mode?
    // puts("This will be shown only in debug mode");

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

    // init:
    runner.m_filler = &filler;
    recorder.m_runner = &runner;
    // TODO: is this correct design?

    // TODO: is this acceptable?
    // recorder.take(maker.make());

    // TODO: must be non-empty; "init" method...
    recorder.take(legacy::game_of_life()); // first rule...
    if (argc == 2) {
        // TODO: add err logger
        recorder.replace(read_rule_from_file(argv[1]));
    }

    // TODO: raii works terribly with C-style cleanup... can texture be destroyed after renderer?
    tile_image img(renderer, runner.tile()); // TODO: ...
    code_image icons(renderer);              // TODO: how to get "renderer" from backend?

    bool paused = false;
    bool show_rule_editor = true;
    bool show_demo_window = false; // TODO: remove this in the future...

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

        ////////////////////////////////////////////////////////////////////////////
        // TODO: experimental...
        // TOO clumsy...
        bool open_popup = false;
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Open##1234")) {
                // if (ImGui::MenuItem("Open##5678", "...")) {}
                open_popup = true;
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        static bool prev_paused = false; // TODO: should be push/pop mode
        if (open_popup) {
            prev_paused = paused;
            paused = true;
            ImGui::OpenPopup("Open file##0123");
        }
        if (ImGui::BeginPopupModal("Open file##0123", NULL, ImGuiWindowFlags_::ImGuiWindowFlags_AlwaysAutoResize)) {
            static char buf[100]{}; // TODO: static? init contents?
            if (ImGui::InputTextWithHint("##1223", "File-path", buf, 100, ImGuiInputTextFlags_EnterReturnsTrue)) {
                recorder.replace(read_rule_from_file(buf));
                buf[0] = '\0';
                ImGui::CloseCurrentPopup();
                paused = prev_paused;
            }
            ImGui::EndPopup();
        }
        /////////////////////////////////////////////////////////////////////////

        // TODO: editor works poorly with recorder...
        // TODO: shouldnt be here...
        if (show_rule_editor) {
            auto edited = edit_rule(show_rule_editor, runner.rule(), icons);
            if (edited != runner.rule()) {
                recorder.take(edited);
            }
        }

        // TODO: remove this when suitable...
        if (show_demo_window) {
            ImGui::ShowDemoWindow(&show_demo_window);
            // ImGui::ShowMetricsWindow(&show_demo_window);
        }

        if (ImGui::Begin("-v-", nullptr,
                         ImGuiWindowFlags_::ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_::ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Checkbox("Rule editor", &show_rule_editor);
            ImGui::SameLine();
            ImGui::Checkbox("Demo window", &show_demo_window);
            ImGui::Text("(%.1f FPS) Frame:%d\n", io.Framerate, ImGui::GetFrameCount());
            ImGui::Separator();

            // tile:
            ImGui::Text("Width:%d,Height:%d,Density:%f", runner.tile().width(), runner.tile().height(),
                        float(runner.tile().count()) / runner.tile().area());
            {
                const ImVec2 pos = ImGui::GetCursorScreenPos();

                img.update(runner.tile());
                SDL_Texture* const img_texture = img.texture();
                const float img_zoom = 1; // TODO: *2 is too big with (320*240)
                const ImVec2 img_size = ImVec2(img.width() * img_zoom, img.height() * img_zoom);

                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
                ImGui::ImageButton(img_texture, img_size);
                ImGui::PopStyleVar();

                // TODO: refine dragging logic...
                if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    // TODO: will not work properly when img_zoom!=1...
                    runner.shift_xy(io.MouseDelta.x, io.MouseDelta.y);
                } else if (ImGui::BeginItemTooltip()) {
                    auto& io = ImGui::GetIO();
                    // TODO: rewrite logic...
                    const float region_sz = 32.0f;
                    float region_x = io.MousePos.x - pos.x - region_sz * 0.5f;
                    float region_y = io.MousePos.y - pos.y - region_sz * 0.5f;
                    const float zoom = 4.0f; // TODO: should be settable between 4 and 8.
                    if (region_x < 0.0f) {
                        region_x = 0.0f;
                    } else if (region_x > img_size.x - region_sz) {
                        region_x = img_size.x - region_sz;
                    }
                    if (region_y < 0.0f) {
                        region_y = 0.0f;
                    } else if (region_y > img_size.y - region_sz) {
                        region_y = img_size.y - region_sz;
                    }
                    ImVec2 uv0 = ImVec2((region_x) / img_size.x, (region_y) / img_size.y);
                    ImVec2 uv1 = ImVec2((region_x + region_sz) / img_size.x, (region_y + region_sz) / img_size.y);
                    ImGui::Image(img_texture, ImVec2(region_sz * zoom, region_sz * zoom), uv0, uv1);

                    ImGui::TextUnformatted("Rclick to copy to clipboard:"); // TODO: show successful / fail...
                    auto rule_str = to_MAP_str(runner.rule());              // how to reuse the resource?
                    ImGui::TextUnformatted(wrap_rule_string(rule_str).c_str());
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_::ImGuiMouseButton_Right)) {
                        ImGui::SetClipboardText(rule_str.c_str()); // TODO: notify...
                    }
                    ImGui::TextUnformatted("Ctrl+Lclick to paste from clipboard:");
                    if (io.KeyCtrl) {
                        std::string found_str;
                        const char* text = ImGui::GetClipboardText();
                        // TODO: can text return nullptr?
                        if (text) {
                            std::string str = text;
                            std::smatch match_result;
                            if (std::regex_search(str, match_result, legacy::regex_MAP_str())) {
                                found_str = match_result[0];
                            }
                        }
                        ImGui::TextUnformatted(found_str.empty() ? "(none)" : wrap_rule_string(found_str).c_str());
                        // TODO: redesign copy/paste... especially lclick-paste is problematic...
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_::ImGuiMouseButton_Left) && !found_str.empty()) {
                            recorder.take(legacy::from_MAP_str(found_str));
                        }
                    }

                    if (io.MouseWheel < 0) { // scroll down
                        recorder.next();
                    } else if (io.MouseWheel > 0) { // scroll up
                        recorder.prev();
                    }

                    ImGui::EndTooltip();
                }

                // TODO: again, can size()==0?
                ImGui::Text("Last:%d At:%d", recorder.size() - 1, recorder.pos());

                static char go_to[20]{};
                auto filter = [](ImGuiInputTextCallbackData* data) {
                    return (data->EventChar >= '0' && data->EventChar <= '9') ? 0 : 1;
                };
                if (ImGui::InputTextWithHint(
                        "##Goto", "GOTO e.g. 2->enter", go_to, 20,
                        ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_EnterReturnsTrue, filter)) {
                    int val{};
                    if (sscanf(go_to, "%d", &val) == 1) {
                        recorder.set_pos(val);
                    }
                    go_to[0] = '\0';
                    // TODO: how to regain focus?
                }
            }

            ImGui::SeparatorText("Rule generator");

            {
                auto b_mode = maker.b_mode;
                auto e_mode = maker.e_mode;
                auto density = maker.density;
                auto interpret_as = maker.interpret_as;

                // is reinter ok?
                ImGui::Combo("##MainMode", underlying_address(b_mode), maker.b_mode_names,
                             std::size(maker.b_mode_names));

                // e_mode:
                ImGui::RadioButton("basic", underlying_address(e_mode), legacy::partition::extra_specification::none);
                ImGui::SameLine();
                ImGui::RadioButton("paired", underlying_address(e_mode), legacy::partition::paired);
                ImGui::SameLine();
                // TODO: explain...
                ImGui::RadioButton("state", underlying_address(e_mode), legacy::partition::state);

                // TODO: too sensitive...
                // TODO: is it suitable to restart immediately?
                char str[40];
                snprintf(str, 40, "Rule density [0-%d]", maker.max_density());
                ImGui::SliderInt(str, &density, 0, maker.max_density(), "%d", ImGuiSliderFlags_NoInput);

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("As");
                ImGui::SameLine();
                ImGui::RadioButton("ABS", underlying_address(interpret_as), legacy::ABS);
                ImGui::SameLine();
                ImGui::RadioButton("XOR", underlying_address(interpret_as), legacy::XOR);

                ImGui::SameLine();
                bool new_rule = ImGui::Button("New rule");

                if (maker.density != density) {
                    maker.density = density;
                    new_rule = true;
                }
                if (maker.b_mode != b_mode || maker.e_mode != e_mode) {
                    maker.b_mode = b_mode;
                    maker.e_mode = e_mode;
                    maker.density = maker.max_density() * 0.3;
                    new_rule = true;
                }
                if (maker.interpret_as != interpret_as) {
                    maker.interpret_as = interpret_as;
                    new_rule = true;
                }

                if (new_rule) {
                    recorder.take(maker.make());
                }
            }

            ImGui::SeparatorText("Tile");
            {
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Gen:%d", runner.gen());
            }

            {
                if (ImGui::SliderFloat("Init density [0-1]", &runner.m_filler->density, 0.0f, 1.0f, "%.3f",
                                       ImGuiSliderFlags_NoInput)) {
                    runner.restart();
                }

                ImGui::Checkbox("Pause", &paused);

                ImGui::SameLine();
                ImGui::PushButtonRepeat(true); // accept consecutive clicks... TODO: too fast...
                if (ImGui::Button("+1")) {
                    runner.run(1);
                }
                ImGui::SameLine();
                if (ImGui::Button("+p")) {
                    runner.run(pergen); // TODO: should combine...
                }
                ImGui::PopButtonRepeat();

                ImGui::SameLine();
                if (ImGui::Button("Restart")) {
                    runner.restart();
                    // runner.reset_tile();
                }
                ImGui::SameLine();
                if (ImGui::Button("Reseed")) {
                    runner.m_filler->disturb();
                    runner.restart();
                }

                // TODO: use char event instead?
                if (ImGui::IsKeyReleased(ImGuiKey_P)) {
                    paused = !paused;
                }
                if (ImGui::IsKeyReleased(ImGuiKey_R)) {
                    runner.restart();
                }
            }

            {
                // TODO: toooo ugly...
                // TODO: (?) currently suitable to restart immediately...
                ImGui::SliderInt("Per gen [1-20]", &pergen, pergen_min, pergen_max, "%d",
                                 ImGuiSliderFlags_NoInput); // TODO: use sprintf for pergen_min and pergen_max,
                                                            // start_min, start_max...
                ImGui::SliderInt("Gap Frame [0-20]", &skip_per_frame, skip_min, skip_max, "%d",
                                 ImGuiSliderFlags_NoInput);
                // ImGui::BeginDisabled();
                ImGui::SliderInt("Start gen [0-1000]", &start_from, start_min, start_max, "%d",
                                 ImGuiSliderFlags_NoInput);
                // ImGui::EndDisabled();
            }
        }
        ImGui::End();

        // run tile (TODO: should be here?)
        if (anti_flick) {
            if (legacy::will_flick(runner.rule()) && pergen % 2) {
                if (pergen == 1) {
                    ++pergen;
                } else {
                    assert(pergen >= 2);
                    --pergen;
                }
            }
            // TODO: how to restore when switch to new rules?
        }
        if (runner.gen() == 0) {
            runner.run(start_from);
        }
        if (!paused) {
            if (ImGui::GetFrameCount() % (skip_per_frame + 1) == 0) {
                runner.run(pergen);
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
    }

    // Cleanup
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
