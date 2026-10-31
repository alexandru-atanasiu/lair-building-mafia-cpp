#include "window_cheats.h"

#if defined(GAME_IMGUI_ENABLED)

#include "debug/debug_pad.h"
#include "platform/window.h"

#include <imgui.h>

#include <algorithm>
#include <array>

namespace
{
    constexpr int kMinWidth  = 320;
    constexpr int kMinHeight = 200;
    constexpr int kMaxSize   = 16384;
}

void RegisterWindowCheats(DebugPad& pad, IWindow& window)
{
    // `resolution` is the pending value in the input fields; 0 = not initialized yet.
    pad.RegisterCategory("Window", [&window, resolution = std::array<int, 2>{}]() mutable {
        bool fullscreen = window.GetMode() == WindowMode::BorderlessFullscreen;
        if (ImGui::Checkbox("Fullscreen", &fullscreen))
            window.SetMode(fullscreen ? WindowMode::BorderlessFullscreen : WindowMode::Windowed);

        ImGui::Text("Current size: %u x %u", window.GetWidth(), window.GetHeight());

        if (window.GetMode() != WindowMode::Windowed)
            return;

        if (resolution[0] == 0)
            resolution = { (int)window.GetWidth(), (int)window.GetHeight() };

        ImGui::InputInt2("Resolution", resolution.data());
        resolution[0] = std::clamp(resolution[0], kMinWidth, kMaxSize);
        resolution[1] = std::clamp(resolution[1], kMinHeight, kMaxSize);

        if (ImGui::Button("Set resolution"))
            window.SetWindowedSize((uint32_t)resolution[0], (uint32_t)resolution[1]);
    });
}

#endif
