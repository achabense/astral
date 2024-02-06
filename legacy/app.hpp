#pragma once

#include "app_imgui.hpp"
#include "rule.hpp"
#include "tile.hpp"

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

std::optional<legacy::moldT> static_constraints();
std::optional<legacy::moldT> edit_rule(const legacy::moldT& mold, const code_image& icons);

// TODO: should be able to load lock as well... (->optional<pair<ruleT,optional<lockT>>>)
std::optional<legacy::ruleT> load_rule(const legacy::ruleT& test_sync);

std::optional<legacy::moldT::lockT> edit_tile(const legacy::ruleT& rule, tile_image& img);

// Never empty.
// TODO: re-apply compression...
class recorderT {
    std::vector<legacy::moldT> m_record;
    int m_pos;

public:
    recorderT() {
        m_record.emplace_back(legacy::game_of_life());
        m_pos = 0;
    }

    int size() const { return m_record.size(); }

    // [0, size() - 1]
    int pos() const { return m_pos; }

    void update(const legacy::moldT& mold) {
        if (mold != m_record[m_pos]) {
            m_record.push_back(mold);
            m_pos = m_record.size() - 1;
        }
    }

    legacy::moldT current() const {
        assert(m_pos >= 0 && m_pos < size());
        return m_record[m_pos];
    }

    void set_pos(int pos) { //
        m_pos = std::clamp(pos, 0, size() - 1);
    }
    void set_next() { set_pos(m_pos + 1); }
    void set_prev() { set_pos(m_pos - 1); }
    void set_first() { set_pos(0); }
    void set_last() { set_pos(size() - 1); }
};

// TODO: rename...
const int FixedItemWidth = 220;

// TODO: support rollbacking diff rules?
// TODO: support rollbacking locks?
// TODO: for editing opt, support in-lock and outof-lock mode?

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
