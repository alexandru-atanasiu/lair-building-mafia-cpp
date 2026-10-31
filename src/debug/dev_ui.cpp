#include "debug/dev_ui.h"

#if defined(GAME_IMGUI_ENABLED)

#include "platform/window.h"
#include "window_cheats.h"

#include <imgui.h>

DevUI::DevUI(IWindow& window)
    : m_window(window)
{
    RegisterWindowCheats(m_debugPad, m_window);
}

void DevUI::Update()
{
    if (!m_visible)
        return;

#if defined(GAME_IMGUI_CONSOLE_ENABLED)
    // The console takes over the whole screen: while it's open nothing else is drawn.
    // The other tools keep their open/closed state and come back when it closes.
    m_console.Update();
    if (m_console.IsOpen())
        return;
#endif

    DrawMainMenuBar();
    m_debugPad.Draw();
    DrawFrameCounter();
}

void DevUI::DrawFrameCounter()
{
    if (!m_showFrameCounter)
        return;

    // Bottom-left of the work area (the game's HUD sits along the top). No background,
    // decorations or input: just text over the scene, and clicks go through to whatever is
    // underneath.
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float          padding  = 8.0f;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + padding, viewport->WorkPos.y + viewport->WorkSize.y - padding),
                            ImGuiCond_Always, ImVec2(0.0f, 1.0f));

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                                   ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin("##FrameCounter", nullptr, flags))
    {
        // 1.5x the normal UI text size (rasterized at that size, so it stays sharp).
        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.5f);

        // ImGui's framerate is a rolling average over the last 60 frames.
        const float fps = ImGui::GetIO().Framerate;
        ImGui::Text("FPS: %d", (int)(fps + 0.5f));
        ImGui::Text("Frame time: %.2f ms", fps > 0.0f ? 1000.0f / fps : 0.0f);

        ImGui::PopFont();
    }
    ImGui::End();
}

void DevUI::DrawMainMenuBar()
{
    if (!ImGui::BeginMainMenuBar())
        return;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Exit"))
            m_window.RequestClose();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Tools"))
    {
        bool debugPadOpen = m_debugPad.IsOpen();
        if (ImGui::MenuItem("DebugPad", nullptr, &debugPadOpen))
            m_debugPad.SetOpen(debugPadOpen);

        ImGui::MenuItem("Frame Counter", nullptr, &m_showFrameCounter);

#if defined(GAME_IMGUI_CONSOLE_ENABLED)
        bool consoleOpen = m_console.IsOpen();
        if (ImGui::MenuItem("Console", "`", &consoleOpen))
            m_console.SetOpen(consoleOpen);
#endif
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

#endif
