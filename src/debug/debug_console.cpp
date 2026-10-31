#include "debug/debug_console.h"

#if defined(GAME_IMGUI_CONSOLE_ENABLED)

#include <imgui.h>

#include <cstring>

namespace
{
    constexpr size_t kMaxLines  = 10000;
    constexpr size_t kTrimLines = 1000; // dropped at once when full, to amortize the erase

    const ImVec4 kWarnColor  = ImVec4(1.00f, 0.85f, 0.30f, 1.0f);
    const ImVec4 kErrorColor = ImVec4(1.00f, 0.40f, 0.40f, 1.0f);

    // Keeps the toggle key from being typed into the filter box.
    int RejectToggleKey(ImGuiInputTextCallbackData* data)
    {
        return data->EventChar == '`' ? 1 : 0;
    }
}

DebugConsole::DebugConsole()
{
    LogAddListener(&DebugConsole::OnLog, this); // replays recent history, so startup logs show up too
}

DebugConsole::~DebugConsole()
{
    LogRemoveListener(&DebugConsole::OnLog, this);
}

void DebugConsole::OnLog(void* user, LogLevel level, const char* line)
{
    auto* self = static_cast<DebugConsole*>(user);
    std::lock_guard<std::mutex> lock(self->m_linesMutex);

    if (self->m_lines.size() >= kMaxLines)
        self->m_lines.erase(self->m_lines.begin(), self->m_lines.begin() + kTrimLines);

    bool continuation = false;
    for (const char* start = line;;)
    {
        const char* end = strchr(start, '\n');
        self->m_lines.push_back({ level, continuation, end ? std::string(start, end) : std::string(start) });
        if (!end)
            break;
        start        = end + 1;
        continuation = true;
    }
}

void DebugConsole::Update()
{
    if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false))
        m_open = !m_open;

    if (m_open)
        Draw();
}

void DebugConsole::Draw()
{
    // Covers the whole screen, no title bar (DevUI hides every other tool while it's open).
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoSavedSettings;
    if (!ImGui::Begin("Console", nullptr, flags))
    {
        ImGui::End();
        return;
    }

    bool clear = ImGui::Button("Clear");
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_autoScroll);
    ImGui::SameLine();
    ImGui::Checkbox("Info", &m_showInfo);
    ImGui::SameLine();
    ImGui::Checkbox("Warn", &m_showWarn);
    ImGui::SameLine();
    ImGui::Checkbox("Error", &m_showError);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##filter", "Filter (\"incl,-excl\")", m_filter, sizeof(m_filter),
                             ImGuiInputTextFlags_CallbackCharFilter, RejectToggleKey);
    ImGui::Separator();

    const ImGuiTextFilter filter(m_filter);

    if (ImGui::BeginChild("##lines", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
    {
        std::lock_guard<std::mutex> lock(m_linesMutex);

        if (clear)
            m_lines.clear();

        // Filters are evaluated on each entry's first line; continuation lines follow it.
        m_visible.clear();
        bool entryShown = false;
        for (int i = 0; i < (int)m_lines.size(); ++i)
        {
            const Line& line = m_lines[i];
            if (!line.continuation)
            {
                const bool levelShown = (line.level == LogLevel::Info && m_showInfo) ||
                                        (line.level == LogLevel::Warn && m_showWarn) ||
                                        (line.level == LogLevel::Error && m_showError);
                entryShown = levelShown && filter.PassFilter(line.text.c_str(), line.text.c_str() + line.text.size());
            }
            if (entryShown)
                m_visible.push_back(i);
        }

        // Every row is a single line, so the clipper only submits the visible ones.
        ImGuiListClipper clipper;
        clipper.Begin((int)m_visible.size());
        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            {
                const Line& line = m_lines[m_visible[i]];
                const bool  colored = line.level != LogLevel::Info;
                if (colored)
                    ImGui::PushStyleColor(ImGuiCol_Text, line.level == LogLevel::Warn ? kWarnColor : kErrorColor);
                ImGui::TextUnformatted(line.text.c_str(), line.text.c_str() + line.text.size());
                if (colored)
                    ImGui::PopStyleColor();
            }
        }

        if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    ImGui::End();
}

#endif
