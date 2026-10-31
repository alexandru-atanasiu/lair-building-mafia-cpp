#include "city_scene.h"

#include "../ui/ui.h"

#include <cstdio>
#include <string>

namespace
{
    constexpr const char* kMapTexture = "ui/city_map.tex";
    constexpr float       kPanelWidth = 280.0f;
    constexpr float       kMargin     = 16.0f;
}

CityScene::CityScene(const SceneServices& services)
    : m_state(services.state)
{
}

void CityScene::Update(const SceneFrame& frame)
{
    Ui& ui = frame.ui;

    const float top = ui.GetContentTop();
    ui.Image({ 0.0f, top, ui.GetWidth(), ui.GetHeight() - top }, ui.Texture(kMapTexture));

    // The crew, in a panel.
    const float rowHeight = 28.0f;
    const float x         = ui.GetWidth() - kPanelWidth - kMargin;
    float       y         = top + kMargin;
    const float height    = 262.0f + rowHeight * (float)m_state.crew.size();
    ui.Panel({ x, y, kPanelWidth, height });

    y += 10.0f;
    Ui::TextStyle title;
    title.size = 20.0f;
    title.bold = true;
    ui.Label({ x + 14.0f, y }, "The City", title);
    y += 32.0f;

    Ui::TextStyle dim;
    dim.size  = 14.0f;
    dim.color = { 0.72f, 0.69f, 0.62f, 1.0f };
    const std::string note = ui.WrapText("Missions will be sent from this screen. For now it shows the crew and the game's widgets.",
                                         kPanelWidth - 28.0f, dim.size);
    ui.Label({ x + 14.0f, y }, note, dim);
    y += 3.0f * ui.GetLineHeight(dim.size) + 10.0f;

    ui.Image({ x + 14.0f, y + 2.0f, 20.0f, 20.0f }, ui.Texture("ui/icons/crew.tex"));
    Ui::TextStyle heading;
    heading.bold = true;
    ui.Label({ x + 40.0f, y + 1.0f }, "Crew", heading);
    y += rowHeight;
    for (const CrewMember& member : m_state.crew)
    {
        ui.Fill({ x + 14.0f, y + 5.0f, 14.0f, 14.0f }, member.tint);
        Ui::TextStyle name;
        name.bold = member.role == CrewRole::Boss;
        ui.Label({ x + 36.0f, y }, member.name, name);
        Ui::TextStyle where = dim;
        where.align = Ui::Align::Right;
        const char* place = member.away ? "Away" : (member.motion.level == WorldLevel::Ground ? "Warehouse" : "Lair");
        ui.Label({ x + kPanelWidth - 14.0f, y + 2.0f }, place, where);
        y += rowHeight;
    }
    y += 8.0f;

    // One of each widget.
    if (ui.Button({ x + 14.0f, y, 170.0f, 34.0f }, "Post a test notice"))
    {
        char text[64];
        snprintf(text, sizeof(text), "Test notice %d from the city.", ++m_clicks);
        m_state.PostEvent(GameEventKind::Info, text);
    }
    if (ui.IconButton({ x + 192.0f, y, 34.0f, 34.0f }, ui.Texture("ui/icons/money.tex"), "An icon button with a tooltip"))
        m_state.PostEvent(GameEventKind::Good, "The icon button was clicked.");
    Ui::ButtonStyle disabled;
    disabled.disabled = true;
    ui.IconButton({ x + 232.0f, y, 34.0f, 34.0f }, ui.Texture("ui/icons/pause.tex"), "Disabled", disabled);
    y += 44.0f;

    const float dayFraction = (float)(m_state.clock.GetHour() * 60 + m_state.clock.GetMinute()) / (24.0f * 60.0f);
    ui.ProgressBar({ x + 14.0f, y, kPanelWidth - 28.0f, 20.0f }, dayFraction, { 0.35f, 0.55f, 0.30f, 1.0f }, "The day so far");
    y += 28.0f;

    const UiRect hoverArea = { x + 14.0f, y, kPanelWidth - 28.0f, 22.0f };
    ui.Fill(hoverArea, { 1.0f, 1.0f, 1.0f, 0.05f });
    ui.LabelIn(hoverArea, "Hover here for a tooltip", dim);
    if (ui.IsHovered(hoverArea))
        ui.Tooltip("Tooltips stay on screen,\nand can have several lines.");
}
