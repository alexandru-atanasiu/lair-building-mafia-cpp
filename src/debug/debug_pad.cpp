#include "debug/debug_pad.h"

#if defined(GAME_IMGUI_ENABLED)

#include "core/assert.h"

#include <imgui.h>

#include <algorithm>

void DebugPad::RegisterCategory(std::string name, DrawFn draw)
{
    GAME_ASSERT(draw, "Category '%s' registered without a draw function", name.c_str());
    GAME_ASSERT(std::none_of(m_categories.begin(), m_categories.end(),
                             [&](const Category& c) { return c.name == name; }),
                "Category '%s' registered twice", name.c_str());

    m_categories.push_back({ std::move(name), std::move(draw) });
}

void DebugPad::UnregisterCategory(const std::string& name)
{
    const size_t removed = std::erase_if(m_categories, [&](const Category& c) { return c.name == name; });
    GAME_ASSERT(removed == 1, "Unregistering unknown category '%s'", name.c_str());
}

void DebugPad::Draw()
{
    if (!m_open)
        return;

    // Right third of the work area (the screen minus the main menu bar), full height.
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float          width    = viewport->WorkSize.x / 3.0f;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - width, viewport->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(width, viewport->WorkSize.y));

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("DebugPad", &m_open, flags))
    {
        if (m_categories.empty())
            ImGui::TextDisabled("No cheat categories registered.");

        for (const Category& category : m_categories)
        {
            ImGui::PushID(category.name.c_str());
            if (ImGui::CollapsingHeader(category.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                category.draw();
            ImGui::PopID();
        }
    }
    ImGui::End();
}

#endif
