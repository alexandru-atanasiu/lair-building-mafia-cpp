#pragma once

#if defined(GAME_IMGUI_ENABLED)

class DebugPad;
class IWindow;

// "Window" DebugPad category: fullscreen toggle and windowed resolution.
// `window` must outlive the registration.
void RegisterWindowCheats(DebugPad& pad, IWindow& window);

#endif
