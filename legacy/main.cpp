// For reference see "example_sdl2_sdlrenderer2/main.cpp":
// https://github.com/ocornut/imgui/blob/master/examples/example_sdl2_sdlrenderer2/main.cpp

// Unfortunately, SDL2-renderer backend doesn't support docking features...
// https://github.com/ocornut/imgui/issues/5835

#include <SDL.h>

#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include "app.hpp"

[[noreturn]] static void resource_failure() {
    printf("Error: %s", SDL_GetError());
    exit(EXIT_FAILURE);
}

static SDL_Window* window = nullptr;
static SDL_Renderer* renderer = nullptr;

static SDL_Texture* create_texture(SDL_TextureAccess access, int w, int h) {
    assert(window && renderer);

    // Using the same pixel format as the one in "imgui_impl_sdlrenderer2.cpp".
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, access, w, h);
    if (!texture) {
        resource_failure();
    }
    return texture;
}

static Uint32 color_for(bool b) {
    // Guaranteed to work under SDL_PIXELFORMAT_XXXX8888.
    return b ? -1 /* White */ : 0 /* Black*/;
}

ImTextureID tile_image::update(const legacy::tileT& tile) {
    if (!m_texture || m_w != tile.width() || m_h != tile.height()) {
        if (m_texture) {
            SDL_DestroyTexture(static_cast<SDL_Texture*>(m_texture));
        }

        m_w = tile.width();
        m_h = tile.height();
        m_texture = create_texture(SDL_TEXTUREACCESS_STREAMING, m_w, m_h);
    }

    void* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(static_cast<SDL_Texture*>(m_texture), nullptr, &pixels, &pitch) != 0) {
        resource_failure();
    }

    tile.for_each_line(tile.range(), [&](int y, std::span<const bool> line) {
        Uint32* p = (Uint32*)((char*)pixels + pitch * y);
        for (bool v : line) {
            *p++ = color_for(v);
        }
    });

    SDL_UnlockTexture(static_cast<SDL_Texture*>(m_texture));

    return m_texture;
}

tile_image::~tile_image() {
    if (m_texture) {
        SDL_DestroyTexture(static_cast<SDL_Texture*>(m_texture));
    }
}

code_image::code_image() {
    const int width = 3, height = 3 * 512;
    m_texture = create_texture(SDL_TEXTUREACCESS_STATIC, width, height);
    // Using heap allocation to avoid "Function uses XXX bytes of stack" warning.
    std::unique_ptr<Uint32[][3][3]> pixels(new Uint32[512][3][3]);
    for_each_code(code) {
        const legacy::envT env = legacy::decode(code);
        const bool fill[3][3] = {{env.q, env.w, env.e}, {env.a, env.s, env.d}, {env.z, env.x, env.c}};
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 3; ++x) {
                pixels[code][y][x] = color_for(fill[y][x]);
            }
        }
    }

    SDL_UpdateTexture(static_cast<SDL_Texture*>(m_texture), nullptr, pixels.get(), width * sizeof(Uint32));
}

code_image::~code_image() { SDL_DestroyTexture(static_cast<SDL_Texture*>(m_texture)); }

// The encoding of `argv` cannot be relied upon, see:
// https://stackoverflow.com/questions/5408730/what-is-the-encoding-of-argv
int main(int, char**) {
    assert(!window && !renderer);

    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        resource_failure();
    }

    // IME: "Input Method Editor"
    // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    // Create window with SDL_Renderer graphics context
    const SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    window = SDL_CreateWindow("Rule editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    if (!window) {
        resource_failure();
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        resource_failure();
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // TODO: Currently the controls of the program are poorly designed, and are especially not taking
    // navigation mode into consideration...
    assert(!(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard));
    assert(!(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableGamepad));

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // TODO: works but blurry, and how to apply in project?
    // ImGui::GetIO().Fonts->AddFontFromFileTTF(R"(C:\Windows\Fonts\Deng.ttf)", 13, nullptr,
    //                                          ImGui::GetIO().Fonts->GetGlyphRangesChineseFull());

    {
        char* base_path = SDL_GetBasePath();
        if (!base_path) {
            resource_failure();
        }

        file_nav_add_special_path(base_path, "Exe path");
        // TODO: remove when finished...
        file_nav_add_special_path(R"(C:\*redacted*\Desktop\rulelists_new)", "Temp");

        const std::string path = base_path;
        SDL_free(base_path);

        const auto strdup = [](const std::string& str) {
            char* buf = new char[str.size() + 1];
            std::copy_n(str.c_str(), str.size() + 1, buf);
            return buf;
        };

        // Freeze the absolute path of "imgui.ini" and "imgui_log.txt".
        // (wontfix) These memory leaks are intentional, as in this case I don't want to care about
        // when to delete them...
        assert(path.ends_with('\\') || path.ends_with('/'));
        ImGui::GetIO().IniFilename = strdup(path + "imgui.ini");
        ImGui::GetIO().LogFilename = strdup(path + "imgui_log.txt");
    }

    auto begin_frame = [] {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                return false;
            }
            // TODO: this appears not needed:
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window)) {
                return false;
            }
        }

        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        return true;
    };

    auto end_frame = [] {
        ImGui::Render();
        const ImGuiIO& io = ImGui::GetIO();
        SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);

        // `SDL_RenderClear` seems not necessary, as the program uses full-screen window.
        // (Kept as it does no harm.)
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(renderer);
    };

    {
        code_image icons;
        tile_image img;

        while (begin_frame()) {
            frame(icons, img);
            end_frame();

            // Added as an extra assurance for the framerate.
            // (Normally `SDL_RENDERER_PRESENTVSYNC` should be enough to guarantee a moderate framerate.)
            static Uint64 next = 0;
            const Uint64 now = SDL_GetTicks64();
            if (now < next) {
                SDL_Delay(next - now);
            }
            next = SDL_GetTicks64() + 10;
        }
    }

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    window = nullptr;
    renderer = nullptr;

    return 0;
}
