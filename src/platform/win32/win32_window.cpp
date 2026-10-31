#include "win32_window.h"

#include "core/fatal.h"
#include "core/log.h"

#if defined(GAME_IMGUI_ENABLED)
#include <imgui.h>
#include <backends/imgui_impl_win32.h>

// Intentionally not declared by the backend header (avoids dragging in <windows.h>).
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

#include <windowsx.h> // GET_X_LPARAM / GET_Y_LPARAM

#include <algorithm>
#include <iterator>

static const wchar_t kWindowClassName[] = L"CapoGameWindowClass";
static constexpr DWORD kWindowedStyle   = WS_OVERLAPPEDWINDOW;
static constexpr DWORD kFullscreenStyle = WS_POPUP;

static DWORD StyleForMode(WindowMode mode)
{
    return mode == WindowMode::BorderlessFullscreen ? kFullscreenStyle : kWindowedStyle;
}

static const char* ModeName(WindowMode mode)
{
    return mode == WindowMode::BorderlessFullscreen ? "borderless fullscreen" : "windowed";
}

// Returns false for keys the game doesn't track.
static bool KeyFromVirtualKey(WPARAM vk, Key& key)
{
    switch (vk)
    {
    case VK_LEFT:  key = Key::Left;  return true;
    case VK_RIGHT: key = Key::Right; return true;
    case VK_UP:    key = Key::Up;    return true;
    case VK_DOWN:  key = Key::Down;  return true;
    case 'W':      key = Key::W;     return true;
    case 'A':      key = Key::A;     return true;
    case 'S':      key = Key::S;     return true;
    case 'D':      key = Key::D;     return true;
    case 'Q':      key = Key::Q;     return true;
    case 'E':      key = Key::E;     return true;
    case VK_SHIFT: key = Key::Shift; return true;
    case VK_CONTROL: key = Key::Control; return true;
    case VK_TAB:   key = Key::Tab;   return true;
    case VK_SPACE: key = Key::Space; return true;
    case VK_ESCAPE: key = Key::Escape; return true;
    case '1':      key = Key::Num1;  return true;
    case '2':      key = Key::Num2;  return true;
    case '3':      key = Key::Num3;  return true;
    }
    return false;
}

// While the window is dragged or resized, a timer with this id drives the modal loop callback.
static constexpr UINT_PTR kModalLoopTimerId = 1;

// The monitor the window is on (mostly), or the primary monitor before the window exists.
static MONITORINFO GetWindowMonitorInfo(HWND hwnd)
{
    HMONITOR    monitor = hwnd ? MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST)
                               : MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi      = { sizeof(mi) };
    GetMonitorInfoW(monitor, &mi);
    return mi;
}

static void RegisterWindowClassOnce(HINSTANCE hinstance, WNDPROC wndProc)
{
    static bool registered = false;
    if (registered)
        return;

    WNDCLASSEXW wc   = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = wndProc;
    wc.hInstance     = hinstance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);
    wc.lpszClassName = kWindowClassName;
    if (!RegisterClassExW(&wc))
        Fatal("RegisterClassExW failed", HRESULT_FROM_WIN32(GetLastError()));
    registered = true;
}

Win32Window::Win32Window(const WindowDesc& desc)
    : m_mode(desc.mode)
    , m_windowedWidth(std::max(desc.width, 1u))
    , m_windowedHeight(std::max(desc.height, 1u))
{
    // Work in physical pixels so "native resolution" really is native.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HINSTANCE hinstance = GetModuleHandleW(nullptr);
    RegisterWindowClassOnce(hinstance, &Win32Window::StaticWndProc);

    wchar_t title[256];
    MultiByteToWideChar(CP_UTF8, 0, desc.title, -1, title, ARRAYSIZE(title));

    // No window yet: it opens on the primary monitor, whose DPI is the system DPI.
    // WM_DPICHANGED handles moves to monitors with another DPI.
    const RECT rc = ComputeWindowRect(GetDpiForSystem());
    m_hwnd = CreateWindowExW(0, kWindowClassName, title, StyleForMode(m_mode),
                             rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                             nullptr, nullptr, hinstance, this);
    if (!m_hwnd)
        Fatal("CreateWindowExW failed", HRESULT_FROM_WIN32(GetLastError()));

    RECT client;
    GetClientRect(m_hwnd, &client);
    m_width  = (uint32_t)(client.right - client.left);
    m_height = (uint32_t)(client.bottom - client.top);

    LOG_INFO("Window created: %ux%u, %s", m_width, m_height, ModeName(m_mode));

    // Raw mouse motion (WM_INPUT): unaccelerated, and not limited by the cursor position.
    RAWINPUTDEVICE mouse = {};
    mouse.usUsagePage = 0x01; // HID_USAGE_PAGE_GENERIC
    mouse.usUsage     = 0x02; // HID_USAGE_GENERIC_MOUSE
    mouse.hwndTarget  = m_hwnd;
    if (!RegisterRawInputDevices(&mouse, 1, sizeof(mouse)))
        LOG_WARN("RegisterRawInputDevices failed (%lu): no mouse deltas", GetLastError());

    ShowWindow(m_hwnd, SW_SHOW);
}

RECT Win32Window::ComputeWindowRect(UINT dpi) const
{
    const MONITORINFO monitor = GetWindowMonitorInfo(m_hwnd);
    if (m_mode == WindowMode::BorderlessFullscreen)
        return monitor.rcMonitor;

    RECT rc = { 0, 0, (LONG)m_windowedWidth, (LONG)m_windowedHeight };
    AdjustWindowRectExForDpi(&rc, kWindowedStyle, FALSE, 0, dpi);
    const LONG w = rc.right - rc.left;
    const LONG h = rc.bottom - rc.top;

    LONG x, y;
    if (m_hasWindowedPosition)
    {
        x = m_windowedPosition.x;
        y = m_windowedPosition.y;
    }
    else
    {
        // Centered in the monitor's work area (but never above/left of it).
        const RECT& work = monitor.rcWork;
        x = std::max(work.left, work.left + ((work.right - work.left) - w) / 2);
        y = std::max(work.top, work.top + ((work.bottom - work.top) - h) / 2);
    }
    return { x, y, x + w, y + h };
}

void Win32Window::ApplyMode()
{
    // A maximized window ignores the size we're about to give it.
    if (IsZoomed(m_hwnd))
        ShowWindow(m_hwnd, SW_RESTORE);

    const RECT rc = ComputeWindowRect(GetDpiForWindow(m_hwnd));
    SetWindowLongPtrW(m_hwnd, GWL_STYLE, (LONG_PTR)(StyleForMode(m_mode) | WS_VISIBLE));
    SetWindowPos(m_hwnd, HWND_TOP, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                 SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
    // WM_SIZE (sent synchronously by SetWindowPos) has updated m_width/m_height.

    LOG_INFO("Window mode: %s, %ux%u", ModeName(m_mode), m_width, m_height);
}

void Win32Window::SaveWindowedPosition()
{
    if (m_mode != WindowMode::Windowed || IsZoomed(m_hwnd) || IsIconic(m_hwnd))
        return;

    RECT rc;
    GetWindowRect(m_hwnd, &rc);
    m_windowedPosition    = { rc.left, rc.top };
    m_hasWindowedPosition = true;
}

void Win32Window::SetMode(WindowMode mode)
{
    if (mode == m_mode)
        return;

    SaveWindowedPosition();
    m_mode = mode;
    ApplyMode();
}

void Win32Window::SetWindowedSize(uint32_t width, uint32_t height)
{
    m_windowedWidth  = std::max(width, 1u);
    m_windowedHeight = std::max(height, 1u);

    if (m_mode == WindowMode::Windowed)
    {
        SaveWindowedPosition(); // resize in place
        ApplyMode();
    }
}

Win32Window::~Win32Window()
{
    SetMouseCaptured(false);

    if (m_hwnd)
        DestroyWindow(m_hwnd);
}

void Win32Window::SetMouseCaptured(bool captured)
{
    if (captured == m_mouseCaptured)
        return;
    m_mouseCaptured = captured;

#if defined(GAME_DEV_TOOLS_ENABLED)
    // Scripted runs must not lock the real cursor.
    if (m_inputInjected)
        return;
#endif

    // ShowCursor keeps a display counter (shown while >= 0). Drive it to a known value rather
    // than stepping it, so it stays right even if something else (e.g. an assert dialog
    // releasing the cursor) changed it in between.
    if (captured)
    {
        // Lock the cursor where it is (a 1x1 clip rectangle) and hide it.
        POINT pos;
        GetCursorPos(&pos);
        const RECT clip = { pos.x, pos.y, pos.x + 1, pos.y + 1 };
        ClipCursor(&clip);
        SetCapture(m_hwnd);
        while (ShowCursor(FALSE) >= 0)
        {
        }
    }
    else
    {
        while (ShowCursor(TRUE) < 0)
        {
        }
        ReleaseCapture();
        ClipCursor(nullptr);
    }
}

void Win32Window::SetKeyState(Key key, bool down)
{
    bool& current = m_keysDown[(size_t)key];
    if (down && !current)
        m_keysPressed[(size_t)key] = true;
    else if (!down && current)
        m_keysReleased[(size_t)key] = true;
    current = down;
}

void Win32Window::SetMouseButtonState(MouseButton button, bool down)
{
    bool& current = m_mouseButtonsDown[(size_t)button];
    if (down && !current)
        m_mouseButtonsPressed[(size_t)button] = true;
    else if (!down && current)
        m_mouseButtonsReleased[(size_t)button] = true;
    current = down;
}

void Win32Window::ReleaseInput()
{
    for (size_t i = 0; i < (size_t)Key::Count; ++i)
        SetKeyState((Key)i, false);
    for (size_t i = 0; i < (size_t)MouseButton::Count; ++i)
        SetMouseButtonState((MouseButton)i, false);
    SetMouseCaptured(false);
}

void Win32Window::ResetPumpInput()
{
    m_mouseWheelDelta = 0.0f;
    m_mouseDeltaX     = 0.0f;
    m_mouseDeltaY     = 0.0f;
    std::fill(std::begin(m_keysPressed), std::end(m_keysPressed), false);
    std::fill(std::begin(m_keysReleased), std::end(m_keysReleased), false);
    std::fill(std::begin(m_mouseButtonsPressed), std::end(m_mouseButtonsPressed), false);
    std::fill(std::begin(m_mouseButtonsReleased), std::end(m_mouseButtonsReleased), false);
}

bool Win32Window::PumpEvents()
{
    // Only count this pump's messages.
    ResetPumpInput();

    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Nothing to render while minimized: sleep until the OS wakes us.
    while (!m_closeRequested && IsIconic(m_hwnd))
    {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0)
        {
            m_closeRequested = true;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return !m_closeRequested;
}

LRESULT CALLBACK Win32Window::StaticWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_NCCREATE)
    {
        // Messages arrive before CreateWindowExW returns, so capture the HWND here.
        auto* cs   = reinterpret_cast<CREATESTRUCTW*>(lparam);
        auto* self = static_cast<Win32Window*>(cs->lpCreateParams);
        self->m_hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    auto* self = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self)
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    return self->WndProc(msg, wparam, lparam);
}

float Win32Window::GetDpiScale() const
{
#if defined(GAME_DEV_TOOLS_ENABLED)
    if (m_dpiScaleOverride > 0.0f)
        return m_dpiScaleOverride;
#endif
    return (float)GetDpiForWindow(m_hwnd) / (float)USER_DEFAULT_SCREEN_DPI;
}

#if defined(GAME_IMGUI_ENABLED)
void Win32Window::ImGuiInitBackend()
{
    ImGui_ImplWin32_Init(m_hwnd);
}

void Win32Window::ImGuiShutdownBackend()
{
    ImGui_ImplWin32_Shutdown();
}

void Win32Window::ImGuiNewFrame()
{
    ImGui_ImplWin32_NewFrame();
}
#endif

#if defined(GAME_DEV_TOOLS_ENABLED)
void Win32Window::DevInjectKey(Key key, bool down)
{
    m_inputInjected = true;
    SetKeyState(key, down);
}

void Win32Window::DevInjectMouseMove(int32_t x, int32_t y)
{
    m_inputInjected = true;
    m_mouseDeltaX += (float)(x - m_mouseX);
    m_mouseDeltaY += (float)(y - m_mouseY);
    m_mouseX = x;
    m_mouseY = y;
}

void Win32Window::DevInjectMouseButton(MouseButton button, bool down)
{
    m_inputInjected = true;
    SetMouseButtonState(button, down);
}

void Win32Window::DevInjectMouseWheel(float notches)
{
    m_inputInjected = true;
    m_mouseWheelDelta += notches;
}
#endif

LRESULT Win32Window::WndProc(UINT msg, WPARAM wparam, LPARAM lparam)
{
#if defined(GAME_DEV_TOOLS_ENABLED)
    // Injected input only: drop the real thing (losing focus must not release injected keys).
    if (m_inputInjected)
    {
        switch (msg)
        {
        case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP:
        case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_RBUTTONDOWN: case WM_RBUTTONUP:
        case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MOUSEMOVE: case WM_MOUSEWHEEL: case WM_INPUT:
            return DefWindowProcW(m_hwnd, msg, wparam, lparam);
        case WM_KILLFOCUS:
            m_hasFocus = false;
            return 0;
        default:
            break;
        }
    }
#endif

#if defined(GAME_IMGUI_ENABLED)
    // No-op until an ImGui context exists.
    if (ImGui_ImplWin32_WndProcHandler(m_hwnd, msg, wparam, lparam))
        return true;
#endif

    switch (msg)
    {
    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED)
        {
            m_width  = LOWORD(lparam);
            m_height = HIWORD(lparam);
        }
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
    {
        Key key;
        if (KeyFromVirtualKey(wparam, key))
            SetKeyState(key, msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN); // repeats don't change the state
        break; // DefWindowProc still handles system keys (Alt+F4)
    }

    case WM_SETFOCUS:
        m_hasFocus = true;
        return 0;

    case WM_KILLFOCUS:
        // Key-up messages go to whichever window has focus now: don't leave keys stuck down.
        m_hasFocus = false;
        ReleaseInput();
        return 0;

    // Dragging or resizing runs a modal loop inside DefWindowProc: PumpEvents doesn't return
    // until it ends. A timer keeps frames coming meanwhile.
    case WM_ENTERSIZEMOVE:
        SetTimer(m_hwnd, kModalLoopTimerId, USER_TIMER_MINIMUM, nullptr);
        return 0;

    case WM_EXITSIZEMOVE:
        KillTimer(m_hwnd, kModalLoopTimerId);
        return 0;

    case WM_TIMER:
        if (wparam != kModalLoopTimerId)
            break;
        if (m_modalLoopCallback)
        {
            m_modalLoopCallback();
            ResetPumpInput(); // consumed by that frame
        }
        return 0;

    case WM_DPICHANGED:
    {
        const UINT dpi = HIWORD(wparam);
        if (m_mode == WindowMode::Windowed && !IsZoomed(m_hwnd))
        {
            // Keep the client size in pixels (it's the game's resolution); only the frame
            // changes with the DPI. Same size in pixels on the new monitor means the window
            // doesn't flip back and forth between DPIs while straddling two monitors.
            const RECT* suggested = reinterpret_cast<const RECT*>(lparam);
            RECT        rc        = { 0, 0, (LONG)m_width, (LONG)m_height };
            AdjustWindowRectExForDpi(&rc, kWindowedStyle, FALSE, 0, dpi);
            SetWindowPos(m_hwnd, nullptr, suggested->left, suggested->top, rc.right - rc.left, rc.bottom - rc.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        LOG_INFO("Window DPI changed: %u (scale %.2f)", dpi, (float)dpi / (float)USER_DEFAULT_SCREEN_DPI);
        return 0;
    }

    case WM_LBUTTONDOWN: case WM_LBUTTONUP:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP:
    {
        const bool down = msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN;
        const MouseButton button = (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) ? MouseButton::Left
                                 : (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP) ? MouseButton::Right
                                                                                   : MouseButton::Middle;
        SetMouseButtonState(button, down);
        return 0;
    }

    case WM_INPUT:
    {
        RAWINPUT raw;
        UINT     size = sizeof(raw);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) != (UINT)-1 &&
            raw.header.dwType == RIM_TYPEMOUSE && !(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
        {
            m_mouseDeltaX += (float)raw.data.mouse.lLastX;
            m_mouseDeltaY += (float)raw.data.mouse.lLastY;
        }
        break; // DefWindowProc does the cleanup
    }

    case WM_MOUSEMOVE:
        m_mouseX = GET_X_LPARAM(lparam);
        m_mouseY = GET_Y_LPARAM(lparam);
        return 0;

    case WM_MOUSEWHEEL:
    {
        // The wheel position is in screen coordinates.
        POINT pt = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
        ScreenToClient(m_hwnd, &pt);
        m_mouseX = pt.x;
        m_mouseY = pt.y;
        m_mouseWheelDelta += (float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA;
        return 0;
    }

    case WM_CLOSE:
        // Don't destroy the HWND here: the render device may still reference it.
        // The window is destroyed in ~Win32Window, after its users are gone.
        m_closeRequested = true;
        return 0;
    }
    return DefWindowProcW(m_hwnd, msg, wparam, lparam);
}

std::unique_ptr<IWindow> CreatePlatformWindow(const WindowDesc& desc)
{
    return std::make_unique<Win32Window>(desc);
}
