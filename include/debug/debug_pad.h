#pragma once

#if defined(GAME_IMGUI_ENABLED)

#include <functional>
#include <string>
#include <vector>

// Cheat panel docked to the right third of the screen, with one collapsible
// section per registered category. Opened from the Tools menu.
class DebugPad
{
public:
    // Draws a category's contents with ImGui. Only called while its section is expanded.
    using DrawFn = std::function<void()>;

    // Category names must be unique. Categories are shown in registration order.
    void RegisterCategory(std::string name, DrawFn draw);
    void UnregisterCategory(const std::string& name);

    bool IsOpen() const { return m_open; }
    void SetOpen(bool open) { m_open = open; }

    // Draws the panel if open. Call between ImGuiLayer::BeginFrame and Render.
    void Draw();

private:
    struct Category
    {
        std::string name;
        DrawFn      draw;
    };

    std::vector<Category> m_categories;
    bool                  m_open = false;
};

#endif
