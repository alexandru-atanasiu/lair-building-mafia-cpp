#pragma once

#if defined(GAME_IMGUI_ENABLED)

#include "debug/debug_console.h"
#include "debug/debug_pad.h"

class IWindow;

// Developer UI: main menu bar (File / Tools), the DebugPad and, if enabled, the log console.
class DevUI
{
public:
    // `window` must outlive the DevUI.
    explicit DevUI(IWindow& window);

    DevUI(const DevUI&)            = delete;
    DevUI& operator=(const DevUI&) = delete;

    // Draws the menu bar and any open tool windows.
    // Call between ImGuiLayer::BeginFrame and Render.
    void Update();

    // Hidden: nothing is drawn (the game looks like ship), e.g. for scripted screenshots.
    bool IsVisible() const { return m_visible; }
    void SetVisible(bool visible) { m_visible = visible; }

    // Systems register their cheat categories here.
    DebugPad& GetDebugPad() { return m_debugPad; }

private:
    void DrawMainMenuBar();
    void DrawFrameCounter();

    IWindow& m_window;
    DebugPad m_debugPad;
    bool     m_showFrameCounter = true;
    bool     m_visible          = true;
#if defined(GAME_IMGUI_CONSOLE_ENABLED)
    DebugConsole m_console;
#endif
};

#endif
