#include "debug/imgui_layer.h"

#if defined(GAME_IMGUI_ENABLED)

#include "core/log.h"
#include "platform/window.h"
#include "render/render_device.h"

#include "../platform/platform_fonts.h"

#include <imgui.h>

namespace
{
    constexpr const char* kFontFile = "consola.ttf"; // Consolas
    constexpr float       kFontSize = 15.0f;         // base size in pixels, before DPI scaling

    // The first font added becomes ImGui's default (and sets style.FontSizeBase).
    void LoadFont()
    {
        char path[1024];
        if (PlatformFindSystemFont(kFontFile, path, sizeof(path)) &&
            ImGui::GetIO().Fonts->AddFontFromFileTTF(path, kFontSize))
        {
            LOG_INFO("ImGui font: %s (%.0f px)", path, kFontSize);
            return;
        }
        LOG_WARN("System font %s not found, using ImGui's built-in font", kFontFile);
    }
}

ImGuiLayer::ImGuiLayer(IWindow& window, IRenderDevice& device)
    : m_window(window)
    , m_device(device)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // No imgui.ini: our tool windows have fixed layouts, and the working directory is the
    // cooked data folder, which shouldn't collect stray files.
    ImGui::GetIO().IniFilename = nullptr;

    ImGui::StyleColorsDark();
    LoadFont();

    m_unscaledStyle = std::make_unique<ImGuiStyle>(ImGui::GetStyle());
    ApplyDpiScale(m_window.GetDpiScale());

    m_window.ImGuiInitBackend();
    m_device.ImGuiInitBackend();

    LOG_INFO("Dear ImGui %s initialized (DPI scale %.2f)", IMGUI_VERSION, m_dpiScale);
}

ImGuiLayer::~ImGuiLayer()
{
    m_device.ImGuiShutdownBackend();
    m_window.ImGuiShutdownBackend();
    ImGui::DestroyContext();
}

void ImGuiLayer::ApplyDpiScale(float dpiScale)
{
    // Rescale from the unscaled style: repeated ScaleAllSizes calls would accumulate rounding.
    ImGuiStyle style   = *m_unscaledStyle;
    style.FontSizeBase = ImGui::GetStyle().FontSizeBase;
    style.ScaleAllSizes(dpiScale);
    style.FontScaleDpi = dpiScale;
    ImGui::GetStyle()  = style;
    m_dpiScale         = dpiScale;
}

void ImGuiLayer::BeginFrame()
{
    // The window may have moved to a monitor with another DPI.
    const float dpiScale = m_window.GetDpiScale();
    if (dpiScale != m_dpiScale)
    {
        ApplyDpiScale(dpiScale);
        LOG_INFO("ImGui DPI scale %.2f", dpiScale);
    }

    m_device.ImGuiNewFrame();
    m_window.ImGuiNewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::Render()
{
    ImGui::Render();
    m_device.ImGuiRender();
}

bool ImGuiLayer::WantsMouse() const
{
    return ImGui::GetIO().WantCaptureMouse;
}

bool ImGuiLayer::WantsKeyboard() const
{
    return ImGui::GetIO().WantCaptureKeyboard;
}

#endif
