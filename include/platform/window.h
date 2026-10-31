#pragma once

#include <cstdint>
#include <functional>
#include <memory>

enum class WindowMode
{
    Windowed,             // decorated window with a client area of width x height
    BorderlessFullscreen, // covers the monitor the window is on, at its native resolution
};

// Keys the game can poll. Add entries as needed (and map them in the platform window).
enum class Key
{
    Left,
    Right,
    Up,
    Down,
    W,
    A,
    S,
    D,
    Q,
    E,
    Shift,
    Control,
    Tab,
    Space,
    Escape,
    Num1,
    Num2,
    Num3,
    Count,
};

enum class MouseButton
{
    Left,
    Right,
    Middle,
    Count,
};

struct WindowDesc
{
    const char* title  = "Capo di Tutti Capi";
    WindowMode  mode   = WindowMode::Windowed;
    uint32_t    width  = 1920; // client size used in Windowed mode
    uint32_t    height = 1080;
};

class IWindow
{
public:
    virtual ~IWindow() = default;

    // Processes pending OS events; blocks while the window is minimized.
    // Returns false once the window has been asked to close.
    virtual bool PumpEvents() = 0;

    // Asks the window to close: PumpEvents returns false from its next call.
    virtual void RequestClose() = 0;

    virtual WindowMode GetMode() const = 0;

    // Switches mode at runtime. Windowed mode uses the windowed client size.
    virtual void SetMode(WindowMode mode) = 0;

    // Sets the client size used in Windowed mode; applied immediately if currently windowed.
    virtual void SetWindowedSize(uint32_t width, uint32_t height) = 0;

    // Current client area size in physical pixels.
    virtual uint32_t GetWidth() const  = 0;
    virtual uint32_t GetHeight() const = 0;

    // True while the window has keyboard focus.
    virtual bool HasFocus() const = 0;

    // Called repeatedly (about every 10 ms) while an OS modal loop blocks PumpEvents, i.e.
    // while the window is being dragged or resized, so the game can keep drawing frames.
    // Input counted per PumpEvents (wheel, mouse deltas, presses/releases) is consumed by each
    // call. The callback must not call PumpEvents. Pass nullptr to remove it.
    virtual void SetModalLoopCallback(std::function<void()> callback) = 0;

    // Input state, updated by PumpEvents. Keys are released when the window loses focus.
    virtual bool IsKeyDown(Key key) const = 0;
    virtual bool IsMouseButtonDown(MouseButton button) const = 0;
    // Transitions during the last PumpEvents (key repeats don't count). A key pressed and
    // released within one pump reports both, even though IsKeyDown never saw it down.
    virtual bool WasKeyPressed(Key key) const = 0;
    virtual bool WasKeyReleased(Key key) const = 0;
    virtual bool WasMouseButtonPressed(MouseButton button) const = 0;
    virtual bool WasMouseButtonReleased(MouseButton button) const = 0;
    // Wheel notches scrolled during the last PumpEvents (positive = away from the user).
    virtual float GetMouseWheelDelta() const = 0;
    // Raw mouse motion during the last PumpEvents, in device counts (right / down positive).
    // Keeps coming while the cursor is captured or stuck against a screen edge.
    virtual float GetMouseDeltaX() const = 0;
    virtual float GetMouseDeltaY() const = 0;
    // Captured: the cursor is hidden and locked in place (for mouse-look); released
    // automatically when the window loses focus.
    virtual void SetMouseCaptured(bool captured) = 0;
    virtual bool IsMouseCaptured() const = 0;
    // Last known cursor position over the client area, in physical pixels from the top-left.
    virtual int32_t GetMouseX() const = 0;
    virtual int32_t GetMouseY() const = 0;

    // Content scale of the monitor the window is on (1.0 = 96 DPI).
    virtual float GetDpiScale() const = 0;

    // Platform handle (HWND on Win32), for render backends.
    virtual void* GetNativeHandle() const = 0;

#if defined(GAME_IMGUI_ENABLED)
    // Dear ImGui platform backend. Driven by ImGuiLayer; requires a current ImGui context.
    virtual void ImGuiInitBackend()     = 0;
    virtual void ImGuiShutdownBackend() = 0;
    virtual void ImGuiNewFrame()        = 0;
#endif

#if defined(GAME_DEV_TOOLS_ENABLED)
    // Simulated input for the dev driver (src/game/dev/). Call between PumpEvents and the frame:
    // it counts as input of that pump. The first call switches the window to injected input for
    // good: real keyboard and mouse messages are ignored from then on, so someone moving the
    // mouse can't disturb a scripted run. Positions are client pixels.
    virtual void DevInjectKey(Key key, bool down) = 0;
    virtual void DevInjectMouseMove(int32_t x, int32_t y) = 0;
    virtual void DevInjectMouseButton(MouseButton button, bool down) = 0;
    virtual void DevInjectMouseWheel(float notches) = 0;
    // Overrides GetDpiScale (> 0), to test UI scaling on any monitor.
    virtual void DevSetDpiScale(float scale) = 0;
#endif
};

std::unique_ptr<IWindow> CreatePlatformWindow(const WindowDesc& desc);
