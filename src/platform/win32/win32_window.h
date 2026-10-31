#pragma once

#include "platform/window.h"

#include <windows.h>

class Win32Window final : public IWindow
{
public:
    explicit Win32Window(const WindowDesc& desc);
    ~Win32Window() override;

    Win32Window(const Win32Window&)            = delete;
    Win32Window& operator=(const Win32Window&) = delete;

    bool       PumpEvents() override;
    void       RequestClose() override { m_closeRequested = true; }
    WindowMode GetMode() const override { return m_mode; }
    void       SetMode(WindowMode mode) override;
    void       SetWindowedSize(uint32_t width, uint32_t height) override;
    uint32_t   GetWidth() const override { return m_width; }
    uint32_t   GetHeight() const override { return m_height; }
    bool       HasFocus() const override { return m_hasFocus; }
    void       SetModalLoopCallback(std::function<void()> callback) override { m_modalLoopCallback = std::move(callback); }
    bool       IsKeyDown(Key key) const override { return m_keysDown[(size_t)key]; }
    bool       IsMouseButtonDown(MouseButton button) const override { return m_mouseButtonsDown[(size_t)button]; }
    bool       WasKeyPressed(Key key) const override { return m_keysPressed[(size_t)key]; }
    bool       WasKeyReleased(Key key) const override { return m_keysReleased[(size_t)key]; }
    bool       WasMouseButtonPressed(MouseButton button) const override { return m_mouseButtonsPressed[(size_t)button]; }
    bool       WasMouseButtonReleased(MouseButton button) const override { return m_mouseButtonsReleased[(size_t)button]; }
    float      GetMouseWheelDelta() const override { return m_mouseWheelDelta; }
    float      GetMouseDeltaX() const override { return m_mouseDeltaX; }
    float      GetMouseDeltaY() const override { return m_mouseDeltaY; }
    void       SetMouseCaptured(bool captured) override;
    bool       IsMouseCaptured() const override { return m_mouseCaptured; }
    int32_t    GetMouseX() const override { return m_mouseX; }
    int32_t    GetMouseY() const override { return m_mouseY; }
    float      GetDpiScale() const override;
    void*      GetNativeHandle() const override { return m_hwnd; }

#if defined(GAME_IMGUI_ENABLED)
    void ImGuiInitBackend() override;
    void ImGuiShutdownBackend() override;
    void ImGuiNewFrame() override;
#endif

#if defined(GAME_DEV_TOOLS_ENABLED)
    void DevInjectKey(Key key, bool down) override;
    void DevInjectMouseMove(int32_t x, int32_t y) override;
    void DevInjectMouseButton(MouseButton button, bool down) override;
    void DevInjectMouseWheel(float notches) override;
    void DevSetDpiScale(float scale) override { m_dpiScaleOverride = scale; }
#endif

private:
    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT WndProc(UINT msg, WPARAM wparam, LPARAM lparam);

    // Outer window rect for the current mode.
    RECT ComputeWindowRect(UINT dpi) const;
    // Re-applies style, position and size for the current mode.
    void ApplyMode();
    // Remembers where the (windowed) window is, so it comes back there.
    void SaveWindowedPosition();
    // Releases keys and buttons (their up messages won't reach us) and the mouse capture.
    void ReleaseInput();
    // Clears the input counted per pump (wheel, mouse deltas, presses/releases).
    void ResetPumpInput();
    void SetKeyState(Key key, bool down);
    void SetMouseButtonState(MouseButton button, bool down);

    HWND       m_hwnd   = nullptr;
    uint32_t   m_width  = 0; // current client size
    uint32_t   m_height = 0;
    bool       m_closeRequested = false;

    WindowMode m_mode           = WindowMode::Windowed;
    uint32_t   m_windowedWidth  = 0;
    uint32_t   m_windowedHeight = 0;
    bool       m_hasWindowedPosition = false; // until set, windowed mode is centered
    POINT      m_windowedPosition    = {};

    bool       m_hasFocus = false;
    std::function<void()> m_modalLoopCallback;

    bool       m_keysDown[(size_t)Key::Count]     = {};
    bool       m_keysPressed[(size_t)Key::Count]  = {};
    bool       m_keysReleased[(size_t)Key::Count] = {};
    bool       m_mouseButtonsDown[(size_t)MouseButton::Count]     = {};
    bool       m_mouseButtonsPressed[(size_t)MouseButton::Count]  = {};
    bool       m_mouseButtonsReleased[(size_t)MouseButton::Count] = {};
    bool       m_mouseCaptured   = false;
    float      m_mouseWheelDelta = 0.0f;
    float      m_mouseDeltaX     = 0.0f;
    float      m_mouseDeltaY     = 0.0f;
    int32_t    m_mouseX = 0;
    int32_t    m_mouseY = 0;

#if defined(GAME_DEV_TOOLS_ENABLED)
    bool       m_inputInjected    = false; // real input is ignored once the dev driver injects
    float      m_dpiScaleOverride = 0.0f;
#endif
};
