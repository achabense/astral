#include "common.hpp"

// Never empty.
class recorderT {
    std::vector<legacy::moldT> m_record;
    int m_pos;

public:
    recorderT() {
        m_record.push_back({.rule = legacy::game_of_life(), .lock{}});
        m_pos = 0;
    }

    int size() const { return m_record.size(); }

    // [0, size() - 1]
    int pos() const { return m_pos; }

    void update(const legacy::moldT& mold) {
        if (mold != m_record[m_pos]) {
            const int last = size() - 1;
            if (m_pos == last && last != 0 && mold == m_record[last - 1]) {
                m_pos = last - 1;
            } else if (m_pos == last - 1 && mold == m_record[last]) {
                m_pos = last;
            } else {
                // TODO: or reverse([m_pos]...[last])? or do nothing?
                if (m_pos != last) {
                    std::swap(m_record[m_pos], m_record[last]);
                }
                m_record.push_back(mold);
                set_last();
            }
        }
    }

    legacy::moldT current() const {
        assert(m_pos >= 0 && m_pos < size());
        return m_record[m_pos];
    }

    void set_next() { set_pos(m_pos + 1); }
    void set_prev() { set_pos(m_pos - 1); }
    void set_first() { set_pos(0); }
    void set_last() { set_pos(size() - 1); }

    void clear() {
        m_record = {m_record[m_pos]};
        m_pos = 0;
    }

private:
    void set_pos(int pos) { m_pos = std::clamp(pos, 0, size() - 1); }
};

static void assign_val(legacy::moldT& mold, legacy::extrT::valT& val) {
    if (val.lock) {
        mold.lock = *val.lock;
        mold.rule = val.rule;
    } else {
        if (!mold.compatible(val.rule)) {
            mold.lock = {};
        }
        mold.rule = val.rule;
    }
}

void frame_main(const code_image& icons, tile_image& img) {
    messenger::display();

#ifndef NDEBUG
    ImGui::ShowDemoWindow();
#endif

    static recorderT recorder;
    legacy::moldT current = recorder.current();
    bool update = false;

    static bool show_load = true;
    static bool show_static = false;

    if (show_load) {
        // TODO: this should be controlled by load_rule ...
        ImGui::SetNextWindowSize({600, 400}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(400, 200), ImVec2(FLT_MAX, FLT_MAX));
        if (auto window = imgui_Window("Load rule", &show_load, ImGuiWindowFlags_NoCollapse)) {
            if (auto out = load_rule()) {
                assign_val(current, *out);
                update = true;
            }
        }
    }
    if (show_static) {
        if (auto window = imgui_Window("Static constraints", &show_static,
                                       ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
            if (auto out = static_constraints()) {
                current = *out;
                update = true;
            }
        }
    }

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    if (auto window = imgui_Window("Main", flags)) {
        if (imgui_KeyPressed(ImGuiKey_H, false)) {
            helper::enable_help = !helper::enable_help;
        }

        ImGui::Checkbox("\"Help\"", &helper::enable_help);
        helper::show_help("Or press 'H' to toggle this mode.");
        ImGui::SameLine();
        ImGui::Checkbox("\"Load\"", &show_load);
        ImGui::SameLine();
        ImGui::Checkbox("\"Static\"", &show_static);
        // TODO: not very useful... show time-since-startup instead?
        // ImGui::SameLine();
        // ImGui::Text("    (%.1f FPS) Frame:%d", ImGui::GetIO().Framerate, ImGui::GetFrameCount());
        // TODO: (temp) added back; remove when pasting is supported by load_rule...
        ImGui::SameLine();
        if (ImGui::Button("Paste")) {
            if (const char* str = ImGui::GetClipboardText()) {
                if (auto out = legacy::extract_MAP_str(std::string_view(str)).val) {
                    assign_val(current, *out);
                    update = true;
                } else {
                    messenger::add_msg("No rule");
                }
            }
        }
        ImGui::SameLine(), imgui_Str("|"), ImGui::SameLine();
        // TODO: simplify? unlike the one in fileT::display, this is mainly for undo/redo...
        iter_group(
            "<|", "prev", "next", "|>", //
            [&] { recorder.set_first(); }, [&] { recorder.set_prev(); }, [&] { recorder.set_next(); },
            [&] { recorder.set_last(); });
        ImGui::SameLine();
        ImGui::Text("Total:%d At:%d", recorder.size(), recorder.pos() + 1); // TODO: +1 is clumsy
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            recorder.clear();
        }

        // TODO: as `current` may have been changed by static_constraints, `current` may have been out-of-sync with
        // recorder at this frame... Does this matter?
        {
            // TODO: add a shortcut for quick rule-saving...
            // (As the rule may be gotten from enter-bound buttons)
            static bool with_lock = false;
            if (with_lock) {
                imgui_StrCopyable(legacy::to_MAP_str(current), imgui_Str);
                with_lock = ImGui::IsItemHovered();
            } else {
                // TODO: relying on "[lock]" not to be hidden...
                imgui_StrCopyable(legacy::to_MAP_str(current.rule), imgui_Str);
                // TODO: about lock...
                helper::show_help("Current rule. You can hover on the text and right-click to copy to the clipboard.");
                ImGui::SameLine();
                imgui_StrDisabled("[lock]");
                with_lock = ImGui::IsItemHovered();
            }
        }

        ImGui::Separator();

        if (ImGui::BeginTable("Layout", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (auto child = imgui_ChildWindow("Rul", {}, 0, ImGuiWindowFlags_NoScrollbar)) {
                if (auto out = edit_rule(current, icons)) {
                    current = *out;
                    update = true;
                }

                // TODO: This is used to pair with enter key and is somewhat broken...
                if (imgui_KeyPressed(ImGuiKey_Semicolon, true)) {
                    recorder.set_prev();
                }
                if (imgui_KeyPressed(ImGuiKey_Apostrophe, true)) {
                    recorder.set_next();
                }
            }
            ImGui::TableNextColumn();
            if (auto child = imgui_ChildWindow("Til", {}, 0, ImGuiWindowFlags_NoScrollbar)) {
                if (auto out = apply_rule(current.rule, img)) {
                    current.lock = *out;
                    update = true;
                }
            }
            ImGui::EndTable();
        }
    }

    if (update) {
        recorder.update(current);
    }
}
