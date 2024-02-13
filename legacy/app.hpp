#pragma once

#include <chrono>

#include "app_imgui.hpp"
#include "rule.hpp"
#include "tile.hpp"

using namespace std::chrono_literals;

// Not necessary?
#if 0
// TODO: it seems explicit u8 encoding guarantee is not strictly needed in this project?
// - Assert that ordinary string literals are encoded with utf-8.
// - u8"..." is not used in this project, as it becomes `char8_t[]` after C++20 (which is not usable).
// - TODO: document ways to pass this check (/utf-8 etc; different compilers)...
inline void assert_utf8_encoding() {
    constexpr auto a = std::to_array("中文");
    constexpr auto b = std::to_array(u8"中文");

    static_assert(std::equal(a.begin(), a.end(), b.begin(), b.end(), [](auto l, auto r) {
        return static_cast<unsigned char>(l) == static_cast<unsigned char>(r);
    }));
}
#endif

inline std::mt19937& global_mt19937() {
    static std::mt19937 rand(time(0));
    return rand;
}

// TODO: redesign (especially as some functions are moved to main.cpp,
// which makes the rest methods a bit confusing...)
class tile_image {
    int m_w, m_h;
    ImTextureID m_texture;

public:
    tile_image(const tile_image&) = delete;
    tile_image& operator=(const tile_image&) = delete;

    tile_image() : m_w{}, m_h{}, m_texture{nullptr} {}

    ~tile_image();
    ImTextureID update(const legacy::tileT& tile);

    ImTextureID texture() const { return m_texture; }
};

class code_image {
    ImTextureID m_texture;

public:
    code_image(const code_image&) = delete;
    code_image& operator=(const code_image&) = delete;

    code_image();
    ~code_image();

    void image(legacy::codeT code, int zoom, const ImVec4& tint_col = ImVec4(1, 1, 1, 1),
               const ImVec4 border_col = ImVec4(0, 0, 0, 0)) const {
        const ImVec2 size(3 * zoom, 3 * zoom);
        const ImVec2 uv0(0, code * (1.0f / 512));
        const ImVec2 uv1(1, (code + 1) * (1.0f / 512));
        ImGui::Image(m_texture, size, uv0, uv1, tint_col, border_col);
    }

    bool button(legacy::codeT code, int zoom, const ImVec4& bg_col = ImVec4(0, 0, 0, 0),
                const ImVec4& tint_col = ImVec4(1, 1, 1, 1)) const {
        const ImVec2 size(3 * zoom, 3 * zoom);
        const ImVec2 uv0(0, code * (1.0f / 512));
        const ImVec2 uv1(1, (code + 1) * (1.0f / 512));
        ImGui::PushID(code);
        const bool hit = ImGui::ImageButton("Code", m_texture, size, uv0, uv1, bg_col, tint_col);
        ImGui::PopID();
        return hit;
    }
};

// TODO: this workaround is ugly...
bool file_nav_add_special_path(const char* u8path, const char* title);
std::optional<legacy::moldT> load_rule(const legacy::moldT& test_sync);

std::optional<legacy::moldT> edit_rule(const legacy::moldT& mold, const code_image& icons);

std::optional<legacy::moldT::lockT> edit_tile(const legacy::ruleT& rule, tile_image& img);

// TODO: better name...
void frame(const code_image& icons, tile_image& img);

// TODO: rename...
const int FixedItemWidth = 220;

// TODO: support rollbacking diff rules?
// TODO: for editing opt, support in-lock and outof-lock mode?
// TODO: Right-click must either to open a submenu, or to toggle on/off the tooltip.

// TODO: reconsider binding and scrolling logic...
inline void iter_pair(const char* tag_first, const char* tag_prev, const char* tag_next, const char* tag_last,
                      auto act_first, auto act_prev, auto act_next, auto act_last, bool allow_binding = true,
                      bool allow_scrolling = true) {
    if (ImGui::Button(tag_first)) {
        act_first();
    }

    ImGui::SameLine();
    ImGui::BeginGroup();
    // TODO: _Left, _Right to toggle?
    if (allow_binding) {
        if (imgui_enterbutton(tag_prev)) {
            act_prev();
        }
        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        if (imgui_enterbutton(tag_next)) {
            act_next();
        }
    } else {
        if (ImGui::Button(tag_prev)) {
            act_prev();
        }
        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        if (ImGui::Button(tag_next)) {
            act_next();
        }
    }

    ImGui::EndGroup();
    if (allow_scrolling && ImGui::IsItemHovered()) {
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

class logger {
    // TODO: refine...
    struct temp_str {
        using clock = std::chrono::steady_clock;
        std::string str;
        clock::time_point deadline;

        temp_str(std::string&& str, std::chrono::milliseconds ms) : str(std::move(str)), deadline(clock::now() + ms) {}

        // TODO: better be expired(now=clock::now) return now>=deadline;
        bool expired() const { return clock::now() >= deadline; }
    };

    static inline std::vector<temp_str> m_tempstrs{};

public:
    logger() = delete;

    // TODO: replace XXXms with variables... (or enums...)
    template <class... U>
    static void add_msg(std::chrono::milliseconds ms, std::format_string<const U&...> fmt, const U&... args) noexcept {
        m_tempstrs.emplace_back(std::format(fmt, args...), ms);
    }

    // TODO: this might combine with itemtooltip...
    static void display() {
        if (!m_tempstrs.empty()) {
            ImGui::BeginTooltip();
            auto pos = m_tempstrs.begin();
            for (auto& temp : m_tempstrs) {
                imgui_str(temp.str);
                if (!temp.expired()) {
                    *pos++ = std::move(temp);
                }
            }
            m_tempstrs.erase(pos, m_tempstrs.end());
            ImGui::EndTooltip();
        }
    }
};
