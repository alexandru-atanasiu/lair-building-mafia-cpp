#pragma once

#if defined(GAME_IMGUI_CONSOLE_ENABLED)

#if !defined(GAME_IMGUI_ENABLED)
#error "GAME_IMGUI_CONSOLE_ENABLED requires GAME_IMGUI_ENABLED"
#endif
#if !defined(GAME_LOGGING_ENABLED)
#error "GAME_IMGUI_CONSOLE_ENABLED requires GAME_LOGGING_ENABLED"
#endif

#include "core/log.h"

#include <mutex>
#include <string>
#include <vector>

// In-game ImGui console showing all log output. Toggled with the ` (grave accent) key.
class DebugConsole
{
public:
    DebugConsole();
    ~DebugConsole();

    DebugConsole(const DebugConsole&)            = delete;
    DebugConsole& operator=(const DebugConsole&) = delete;

    // Handles the toggle key and draws the console if open.
    // Call between ImGuiLayer::BeginFrame and Render.
    void Update();

    bool IsOpen() const { return m_open; }
    void SetOpen(bool open) { m_open = open; }

private:
    // One physical line. Multi-line log entries (e.g. asserts with callstacks) are split so
    // every row has the same height; continuation lines follow their entry's first line.
    struct Line
    {
        LogLevel    level;
        bool        continuation;
        std::string text;
    };

    static void OnLog(void* user, LogLevel level, const char* line);
    void Draw();

    std::mutex        m_linesMutex; // logs can arrive from any thread
    std::vector<Line> m_lines;
    std::vector<int>  m_visible;    // scratch: indices of lines passing the filters

    bool m_open       = false;
    bool m_autoScroll = true;
    bool m_showInfo   = true;
    bool m_showWarn   = true;
    bool m_showError  = true;
    char m_filter[256] = {};
};

#endif
