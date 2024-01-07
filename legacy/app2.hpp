#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>

#include "app.hpp"
#include "imgui.h"

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

// TODO: move elsewhere...
// - Experience in MSVC
// - It turns out that there are still a lot of messy encoding problems even if "/utf-8" is specified.
//   (For example, how is `exception.what()` encoded? What does `path` expects from `string`? And what about
//   `filesystem.path.string()`?)
inline std::string cpp17_u8string(const std::filesystem::path& p) {
    return reinterpret_cast<const char*>(p.u8string().c_str());
}

// Unlike ImGui::TextWrapped, doesn't take fmt str...
// TODO: the name is awful...
inline void imgui_str(std::string_view str) {
    ImGui::TextUnformatted(str.data(), str.data() + str.size());
}

// TODO: whether to apply std::format here?
// template <class... U>
// inline void imgui_strfmt(std::format_string<const U&...> fmt, const U&... args) {
//     imgui_str(std::format(fmt, args...));
// }

inline void imgui_strwrapped(std::string_view str) {
    ImGui::PushTextWrapPos(0.0f);
    imgui_str(str);
    ImGui::PopTextWrapPos();
}

// TODO: imgui_strcolored?

inline void imgui_strdisabled(std::string_view str) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    imgui_str(str);
    ImGui::PopStyleColor();
}

inline bool imgui_keypressed(ImGuiKey key, bool repeat) {
    return !ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(key, repeat);
};

// TODO: other mouse functions...
inline bool imgui_scrolling() {
    return ImGui::GetIO().MouseWheel != 0;
}

inline bool imgui_scrolldown() {
    return ImGui::GetIO().MouseWheel < 0;
}

inline bool imgui_scrollup() {
    return ImGui::GetIO().MouseWheel > 0;
}

struct [[nodiscard]] imgui_window {
    imgui_window(const imgui_window&) = delete;
    imgui_window& operator=(const imgui_window&) = delete;

    const bool visible;
    // TODO: refine interface and documentation
    explicit imgui_window(const char* name, ImGuiWindowFlags flags = {})
        : visible(ImGui::Begin(name, nullptr, flags)) {}
    explicit imgui_window(const char* name, bool* p_open, ImGuiWindowFlags flags = {})
        : visible(ImGui::Begin(name, p_open, flags)) {}
    ~imgui_window() {
        ImGui::End(); // Unconditional.
    }
    explicit operator bool() const { return visible; }
};

struct [[nodiscard]] imgui_childwindow {
    imgui_childwindow(const imgui_childwindow&) = delete;
    imgui_childwindow& operator=(const imgui_childwindow&) = delete;

    const bool visible;
    explicit imgui_childwindow(const char* name, const ImVec2& size = {}, ImGuiChildFlags child_flags = {},
                               ImGuiWindowFlags window_flags = {})
        : visible(ImGui::BeginChild(name, size, child_flags, window_flags)) {}
    ~imgui_childwindow() {
        ImGui::EndChild(); // Unconditional.
    }
    explicit operator bool() const { return visible; }
};

#if 0
struct timeT {
    int year;
    int month; // [1,12]
    int day;   // [1,31]
    int hour;  // [0,23]
    int min;   // [0,59]
    int sec;   // [0,59]
    int ms;

    static timeT now() {
        using namespace std::chrono;
        auto now = current_zone()->to_local(system_clock::now());

        year_month_day ymd(floor<days>(now));
        int year = ymd.year().operator int();
        int month = ymd.month().operator unsigned int();
        int day = ymd.day().operator unsigned int();

        hh_mm_ss hms(floor<milliseconds>(now - floor<days>(now)));
        int hour = hms.hours().count();
        int min = hms.minutes().count();
        int sec = hms.seconds().count();
        int ms = hms.subseconds().count();
        return timeT{year, month, day, hour, min, sec, ms};
    }
};
#endif

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

    template <class... U>
    static void log_temp(std::chrono::milliseconds ms, std::format_string<const U&...> fmt, const U&... args) noexcept {
        m_tempstrs.emplace_back(std::format(fmt, args...), ms);
    }

    // TODO: this might combine with itemtooltip...
    static void tempwindow() {
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

// TODO: still, avoid using fstream if possible...
// - Why using C++ at all: there seems no standard way to `fopen` a unicode C-string path.
// (The only one being ignore(max)->gcount, which appears )
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
    logger::log_temp(300ms, "Cannot load");
    return {};
}

inline std::vector<legacy::compressT> read_rule_from_file(const std::filesystem::path& path) {
    return extract_rules(load_binary(path, 1'000'000));
}
