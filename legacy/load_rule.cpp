// TODO: not used in this source file...
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <filesystem>
#include <fstream>

#include "app.hpp"

// - Experience in MSVC
// - It turns out that there are still a lot of messy encoding problems even if "/utf-8" is specified.
//   (For example, how is `exception.what()` encoded? What does `path` expects from `string`? And what about
//   `filesystem.path.string()`?)

// (wontfix) After wasting so much time, I'd rather afford the extra copy than bothering with "more efficient"
// implementations any more.

static std::string cpp17_u8string(const std::filesystem::path& p) {
    const auto u8string = p.u8string();
    return std::string(u8string.begin(), u8string.end());
}

// As to why not using `filesystem::u8path`:
// There is no standard way to shut the compiler up for a [[deprecated]] warning.
// As to making an `std::u8string` first, see:
// https://stackoverflow.com/questions/57603013/how-to-safely-convert-const-char-to-const-char8-t-in-c20
// In short, in C++20 it's impossible to get `char8_t*` from `char*` without copy and in a well-defined way.
static std::filesystem::path cpp17_u8path(const std::string_view path) { //
    return std::filesystem::path(std::u8string(path.begin(), path.end()));
}

// TODO: recheck...
// TODO: able to create/append/open file?
class file_nav {
    using path = std::filesystem::path;
    using entries = std::vector<std::filesystem::directory_entry>;

    char buf_path[200]{};
    char buf_filter[20]{".txt"};

    // TODO: is canonical path necessary?
    bool valid = false; // The last call to `collect(current, ...)` is successful.
    path current;       // Canonical path.
    entries dirs, files;

    static void collect(const path& p, entries& dirs, entries& files) {
        dirs.clear();
        files.clear();
        for (const auto& entry :
             std::filesystem::directory_iterator(p, std::filesystem::directory_options::skip_permission_denied)) {
            const auto status = entry.status();
            if (is_directory(status)) {
                dirs.emplace_back(entry);
            }
            if (is_regular_file(status)) {
                files.emplace_back(entry);
            }
        }
    }

    // Intentionally pass by value.
    void set_current(path p) {
        try {
            p = std::filesystem::canonical(p);
            entries p_dirs, p_files;
            collect(p, p_dirs, p_files);

            valid = true;
            current.swap(p);
            dirs.swap(p_dirs);
            files.swap(p_files);
        } catch (const std::exception&) {
            // TODO: report error... what encoding?
        }
    }

    void refresh_if_valid() {
        if (valid) {
            try {
                collect(current, dirs, files);
            } catch (const std::exception&) {
                // TODO: report error... what encoding?
                valid = false;
                dirs.clear();
                files.clear();
            }
        }
    }

public:
    file_nav(path p = std::filesystem::current_path()) {
        valid = false;
        set_current(std::move(p));
    }

    // TODO: refine...
    inline static std::vector<std::pair<path, std::string>> additionals;
    static bool add_special_path(const char* u8path, const char* title) { //
        std::error_code ec{};
        path p = std::filesystem::canonical(cpp17_u8path(u8path), ec);
        if (!ec) {
            additionals.emplace_back(std::move(p), title);
            return true;
        }
        return false;
    }

    [[nodiscard]] std::optional<path> display() {
        std::optional<path> target = std::nullopt;

        if (ImGui::SmallButton("Refresh")) {
            refresh_if_valid();
        }

        if (valid) {
            imgui_str(cpp17_u8string(current));
        } else {
            assert(dirs.empty() && files.empty());
            imgui_strdisabled("(Invalid) " + cpp17_u8string(current));
        }

        ImGui::Separator();

        if (ImGui::BeginTable("##Table", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            {
                // TODO: better hint...
                if (ImGui::InputTextWithHint("Path", "-> enter", buf_path, std::size(buf_path),
                                             ImGuiInputTextFlags_EnterReturnsTrue)) {
                    // TODO: is the usage of / correct?
                    // TODO: is this still ok when !valid?
                    set_current(current / cpp17_u8path(buf_path));
                    buf_path[0] = '\0';
                }
                for (const auto& [path, title] : additionals) {
                    if (ImGui::MenuItem(title.c_str())) {
                        set_current(path);
                    }
                }
                if (ImGui::MenuItem("..")) {
                    set_current(current.parent_path());
                }
                ImGui::Separator();
                if (auto child = imgui_childwindow("Folders")) {
                    if (dirs.empty()) {
                        imgui_strdisabled("None");
                    }
                    const std::filesystem::directory_entry* sel = nullptr;
                    for (const auto& entry : dirs) {
                        // TODO: cache str?
                        const std::string str = cpp17_u8string(entry.path().filename());
                        if (ImGui::Selectable(str.c_str())) {
                            sel = &entry;
                        }
                    }
                    // TODO: explain this is the reason why `set_current` must take by value...
                    if (sel) {
                        // TODO: does directory_entry.path always return absolute path?
                        assert(sel->path().is_absolute());
                        set_current(sel->path());
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
                        const std::string str = cpp17_u8string(entry.path().filename());
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

bool file_nav_add_special_path(const char* u8path, const char* title) {
    return file_nav::add_special_path(u8path, title);
}

#if 0
// TODO: path must be a regular file...
// TODO: fileT loading may be better of going through load_binary...
inline std::vector<char> load_binary(const std::filesystem::path& path, int max_size) {
    std::error_code ec{};
    const auto size = std::filesystem::file_size(path, ec);
    if (size != -1 && size < max_size) {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (file) {
            std::vector<char> data(size);
            file.read(data.data(), size);
            if (file && file.gcount() == size) {
                return data;
            }
        }
    }
    // TODO: refine msg?
    messenger::add_msg("Cannot load");
    return {};
}
#endif

// TODO: should support clipboard paste in similar ways...
// TODO: support saving into file? (without relying on the clipboard)
// TODO: support in-memory loadings. (e.g. tutorial about typical ways to find interesting rules)

// TODO: refine...
struct fileT {
    struct lineT {
        std::string text;
        std::optional<int> id = std::nullopt; // TODO: (temp) special value or optional?
    };

    std::filesystem::path m_path;
    std::vector<lineT> m_lines;
    std::vector<legacy::extrT::valT> m_rules; // TODO: whether to do compression?
    int pointing_at = 0;                      // valid if !m_rules.empty().

    bool need_reset_scroll = true;

    // TODO: It is easy to locate all rules in a text span via `extract_MAP_str`.
    // However there are no easy ways to locate or highlight (only) the rule across the lines in the gui.
    // See: https://github.com/ocornut/imgui/issues/2313
    // So, currently the program only recognizes the first rule for each line, and highlights the whole line if
    // the line contains a rule.

    static void append(std::vector<lineT>& lines, std::vector<legacy::extrT::valT>& rules, std::istream& is) {
        std::string text;
        while (std::getline(is, text)) {
            const auto val = legacy::extract_MAP_str(text).val;
            lineT& line = lines.emplace_back(std::move(text));
            if (val.has_value()) {
                rules.push_back(*val);
                line.id = rules.size() - 1;
            }
        }
    }

    fileT(std::filesystem::path path) : m_path(std::move(path)) { reload(); }

    void reload() {
        m_lines.clear();
        m_rules.clear();
        pointing_at = 0;

        std::ifstream ifs(m_path);
        append(m_lines, m_rules, ifs);
        need_reset_scroll = true;
    }

    std::optional<legacy::extrT::valT> display() {
        bool hit = false; // TODO: rename...
        const int total = m_rules.size();

        if (total != 0) {
            ImGui::BeginGroup();
            iter_pair(
                "<|", "prev", "next", "|>",                                              //
                [&] { hit = true, pointing_at = 0; },                                    //
                [&] { hit = true, pointing_at = std::max(0, pointing_at - 1); },         //
                [&] { hit = true, pointing_at = std::min(total - 1, pointing_at + 1); }, //
                [&] { hit = true, pointing_at = total - 1; }, false, false);
            ImGui::SameLine();
            ImGui::Text("Total:%d At:%d", total, pointing_at + 1);
            ImGui::EndGroup();
            if (ImGui::IsItemClicked()) {
                hit = true;
            }

            // TODO: (temp) without ImGuiFocusedFlags_ChildWindows, clicking the child window will invalidate this.
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
                if (imgui_keypressed(ImGuiKey_UpArrow, true)) {
                    hit = true, pointing_at = std::max(0, pointing_at - 1);
                }
                if (imgui_keypressed(ImGuiKey_DownArrow, true)) {
                    hit = true, pointing_at = std::min(total - 1, pointing_at + 1);
                }
            }
        } else {
            ImGui::Text("Not found");
        }

        const bool focus = hit;

        ImGui::Separator();

        if (need_reset_scroll) {
            ImGui::SetNextWindowScroll({0, 0});
            assert(pointing_at == 0);
            if (total != 0) {
                hit = true; // TODO: whether to set hit in this case?
            }
            need_reset_scroll = false;
        }
        if (auto child = imgui_childwindow("Child", {}, 0, ImGuiWindowFlags_None)) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            // TODO: refine line-no logic (line-no or id-no)?
            for (int l = 1; const auto& [text, id] : m_lines) {
                ImGui::TextDisabled("%2d ", l++);
                ImGui::SameLine();
                imgui_strwrapped(text);

                // TODO: use different colors for ruleT/moldT?
                if (id.has_value()) {
                    if (id == pointing_at) {
                        const ImVec2 pos_min = ImGui::GetItemRectMin();
                        const ImVec2 pos_max = ImGui::GetItemRectMax();
                        ImGui::GetWindowDrawList()->AddRectFilled(pos_min, pos_max, IM_COL32(0, 255, 0, 60));
                        if (!ImGui::IsItemVisible() && focus) {
                            ImGui::SetScrollHereY();
                        }
                    }
                    if (ImGui::IsItemHovered()) {
                        const ImVec2 pos_min = ImGui::GetItemRectMin();
                        const ImVec2 pos_max = ImGui::GetItemRectMax();
                        ImGui::GetWindowDrawList()->AddRectFilled(pos_min, pos_max, IM_COL32(0, 255, 0, 30));
                        if (ImGui::IsItemClicked()) {
                            pointing_at = *id;
                            hit = true;
                        }
                    }
                }
            }
            ImGui::PopStyleVar();
        }

        if (hit) {
            assert(pointing_at >= 0 && pointing_at < total);
            return m_rules[pointing_at];
        } else {
            return std::nullopt;
        }
    }
};

// TODO: show the last opened file in file-nav?

std::optional<legacy::extrT::valT> load_rule() {
    static file_nav nav;
    static std::optional<fileT> file;

    std::optional<legacy::extrT::valT> out;

    bool close = false;
    if (file) {
        // TODO: the path (& file_nav's) should be copyable...

        // TODO (temp) selectable looks good but is easy to click by mistake...
        if (ImGui::SmallButton("Close")) {
            close = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Reload")) {
            file->reload();
            // TODO: what if file->m_rules.empty() now?
        }
        imgui_str(cpp17_u8string(file->m_path));

        out = file->display();
    } else {
        if (auto sel = nav.display()) {
            file.emplace(*sel);
            if (file->m_rules.empty()) {
                file.reset();
                messenger::add_msg("No rules"); // TODO: better msg...
            }
        }
    }

    if (close) {
        file.reset();
    }

    // TODO: whether to support opening multiple files?
    // TODO: better layout...

    return out;
}
