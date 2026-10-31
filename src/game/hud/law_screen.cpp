#include "law_screen.h"

#include "../sim/game_state.h"
#include "../ui/ui.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace
{
    constexpr float kMaxPanelWidth = 640.0f;
    constexpr float kMargin        = 20.0f;
    constexpr float kPadding       = 24.0f;
    constexpr float kTitleSize     = 32.0f;
    constexpr float kLineSize      = 16.0f;
    constexpr float kLineGap       = 8.0f;
    constexpr float kButtonWidth   = 180.0f;
    constexpr float kButtonHeight  = 38.0f;
}

void DrawLawScreen(Ui& ui, GameState& state)
{
    LawSim& law = state.law;
    if (law.screen == LawScreen::None)
        return;
    const bool busted = law.screen == LawScreen::Busted;

    // Over the whole window: nothing under it can be clicked.
    Ui::PanelStyle dim;
    dim.fill  = { 0.02f, 0.02f, 0.03f, 0.78f };
    dim.frame = { 0.0f, 0.0f, 0.0f, 0.0f };
    ui.Panel({ -4.0f, -4.0f, ui.GetWidth() + 8.0f, ui.GetHeight() + 8.0f }, dim);

    const float width     = std::min(kMaxPanelWidth, ui.GetWidth() - 2.0f * kMargin);
    const float textWidth = width - 2.0f * kPadding;

    std::vector<std::string> lines;
    float height = kPadding + ui.GetLineHeight(kTitleSize) + 12.0f;
    for (const std::string& line : law.report.lines)
    {
        lines.push_back(ui.WrapText(line, textWidth, kLineSize));
        const int rows = 1 + (int)std::count(lines.back().begin(), lines.back().end(), '\n');
        height += (float)rows * ui.GetLineHeight(kLineSize) + kLineGap;
    }
    height += 10.0f + kButtonHeight + kPadding;

    const UiRect panelRect = { 0.5f * (ui.GetWidth() - width), std::max(kMargin, 0.5f * (ui.GetHeight() - height)), width, height };
    Ui::PanelStyle panel;
    panel.fill  = { 0.10f, 0.08f, 0.07f, 0.98f };
    panel.frame = busted ? Color{ 0.80f, 0.20f, 0.14f, 1.0f } : Color{ 0.75f, 0.60f, 0.25f, 1.0f };
    ui.Panel(panelRect, panel);

    float y = panelRect.y + kPadding;
    Ui::TextStyle title;
    title.size  = kTitleSize;
    title.bold  = true;
    title.color = busted ? Color{ 0.95f, 0.35f, 0.25f, 1.0f } : Color{ 0.95f, 0.80f, 0.40f, 1.0f };
    ui.Label({ panelRect.x + kPadding, y }, law.report.title, title);
    y += ui.GetLineHeight(kTitleSize) + 12.0f;

    Ui::TextStyle text;
    text.size = kLineSize;
    for (const std::string& line : lines)
    {
        ui.Label({ panelRect.x + kPadding, y }, line, text);
        const int rows = 1 + (int)std::count(line.begin(), line.end(), '\n');
        y += (float)rows * ui.GetLineHeight(kLineSize) + kLineGap;
    }
    y += 10.0f;

    const UiRect button = { panelRect.x + width - kPadding - kButtonWidth, y, kButtonWidth, kButtonHeight };
    law.screenButtonX = (int)((button.x + 0.5f * button.w) * ui.GetScale());
    law.screenButtonY = (int)((button.y + 0.5f * button.h) * ui.GetScale());
    if (ui.Button(button, busted ? "New game" : "Move in"))
        law.DismissScreen(state);
}
