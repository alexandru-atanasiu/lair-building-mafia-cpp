#pragma once

#if defined(GAME_IMGUI_ENABLED)

#include <memory>

class IWindow;
class IRenderDevice;
struct ImGuiStyle;

// Owns the Dear ImGui context and drives the window/render device backends.
// Must be destroyed before the window and device it was given.
class ImGuiLayer
{
public:
    ImGuiLayer(IWindow& window, IRenderDevice& device);
    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&)            = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    // Starts a new ImGui frame. ImGui:: calls are valid between BeginFrame and Render.
    void BeginFrame();

    // Finalizes the frame and draws it into the current back buffer
    // (call between IRenderDevice::BeginFrame and EndFrame, after the scene).
    void Render();

    // True while ImGui uses the mouse (cursor over a window, dragging) or the keyboard
    // (text input): the game should ignore that input. Valid after BeginFrame.
    bool WantsMouse() const;
    bool WantsKeyboard() const;

private:
    // Scales the UI for the monitor the window is on.
    void ApplyDpiScale(float dpiScale);

    IWindow&                    m_window;
    IRenderDevice&              m_device;
    std::unique_ptr<ImGuiStyle> m_unscaledStyle; // style at DPI scale 1, rescaled on DPI changes
    float                       m_dpiScale = 0.0f;
};

#endif
