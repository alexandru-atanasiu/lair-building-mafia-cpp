#include "hud.h"

#include "../ui/ui.h"
#include "platform/window.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>

namespace
{
    constexpr float kBarHeight   = 46.0f;
    constexpr float kButtonSize  = 32.0f;
    constexpr float kViewButtonW = 118.0f;
    constexpr float kNewsSeconds = 6.0f; // real seconds a news line stays
    constexpr float kHeatWarningSeconds = 5.0f; // real seconds the heat bar flashes after a rise
    constexpr float kHeatJump           = 0.5f; // less than this isn't a rise worth a warning
    constexpr float kEvidenceBarW       = 128.0f;
    constexpr float kEscapesW           = 84.0f;
    constexpr float kNarrowHudBelow     = 1270.0f; // logical width under which the view buttons lose their labels
    constexpr float kNarrowViewButtonW  = 38.0f;

    struct ViewButton
    {
        ViewId      view;
        const char* label;
        const char* icon;
    };
    constexpr ViewButton kViewButtons[] = {
        { ViewId::Warehouse, "Warehouse", "ui/icons/warehouse.tex" },
        { ViewId::Lair, "Lair", "ui/icons/lair.tex" },
        { ViewId::City, "City", "ui/icons/city.tex" },
    };

    // Grouped thousands: $12,345
    std::string FormatMoney(int64_t money)
    {
        std::string digits = std::to_string(money < 0 ? -money : money);
        for (int i = (int)digits.size() - 3; i > 0; i -= 3)
            digits.insert((size_t)i, ",");
        return (money < 0 ? "-$" : "$") + digits;
    }

    Color HeatColor(float heat)
    {
        const float t = heat / 100.0f;
        return { 0.85f, 0.75f * (1.0f - t) + 0.12f, 0.15f, 1.0f };
    }
}

ViewId Hud::Update(Ui& ui, GameState& state, const IWindow& window, bool keyboardAvailable, ViewId active, float topInset)
{
    ViewId view = active;

    if (keyboardAvailable)
    {
        if (window.WasKeyPressed(Key::Space))
            state.clock.SetPaused(!state.clock.IsPaused());
        const Key speedKeys[] = { Key::Num1, Key::Num2, Key::Num3 };
        for (int i = 0; i < GameClock::kMaxSpeed; ++i)
        {
            if (window.WasKeyPressed(speedKeys[i]))
            {
                state.clock.SetSpeed(i + 1);
                state.clock.SetPaused(false);
            }
        }
        if (window.WasKeyPressed(Key::Tab))
            view = (ViewId)(((int)active + 1) % (int)ViewId::Count);
    }

    Ui::PanelStyle panel;
    panel.fill = { 0.08f, 0.07f, 0.06f, 0.95f };
    ui.Panel({ -2.0f, topInset - 2.0f, ui.GetWidth() + 4.0f, kBarHeight + 2.0f }, panel);

    const float buttonY = topInset + 0.5f * (kBarHeight - kButtonSize);
    const float textY   = topInset + 0.5f * (kBarHeight - ui.GetLineHeight(20.0f));
    float       x       = 14.0f;

    Ui::TextStyle value;
    value.size = 20.0f;
    value.bold = true;

    // Money
    ui.Image({ x, topInset + 12.0f, 22.0f, 22.0f }, ui.Texture("ui/icons/money.tex"), { 0.55f, 0.80f, 0.40f, 1.0f });
    x += 28.0f;
    value.color = { 0.65f, 0.90f, 0.50f, 1.0f };
    const std::string money = FormatMoney(state.money);
    ui.Label({ x, textY }, money, value);
    x += std::max(ui.MeasureText(money, value.size, true), 96.0f) + 20.0f;

    // Heat
    ui.Image({ x, topInset + 12.0f, 22.0f, 22.0f }, ui.Texture("ui/icons/heat.tex"), HeatColor(state.heat));
    x += 28.0f;
    if (state.heat > m_lastHeat + kHeatJump)
    {
        m_heatRise    = m_heatWarning > 0.0f ? m_heatRise + state.heat - m_lastHeat : state.heat - m_lastHeat;
        m_heatWarning = kHeatWarningSeconds;
    }
    m_lastHeat = state.heat;

    char heatText[32];
    snprintf(heatText, sizeof(heatText), "%d  %s", (int)(state.heat + 0.5f), GetHeatBandName(state.heat));
    const UiRect heatBar = { x, topInset + 13.0f, 134.0f, 20.0f };
    ui.ProgressBar(heatBar, state.heat / 100.0f, HeatColor(state.heat));
    for (float band : { 0.25f, 0.5f, 0.75f }) // where missions get harder
        ui.Fill({ heatBar.x + heatBar.w * band, heatBar.y + 1.0f, 1.0f, heatBar.h - 2.0f }, { 0.0f, 0.0f, 0.0f, 0.55f });
    Ui::TextStyle heatStyle;
    heatStyle.size = 14.0f;
    heatStyle.bold = true;
    ui.LabelIn(heatBar, heatText, heatStyle);
    if (m_heatWarning > 0.0f)
    {
        const float pulse = 0.5f + 0.5f * std::sin(m_heatWarning * 8.0f);
        ui.Frame({ heatBar.x - 2.0f, heatBar.y - 2.0f, heatBar.w + 4.0f, heatBar.h + 4.0f }, 2.0f, { 1.0f, 0.62f + 0.3f * pulse, 0.2f, 1.0f });
        char rise[16];
        snprintf(rise, sizeof(rise), "+%d", (int)std::ceil(m_heatRise));
        Ui::TextStyle riseStyle;
        riseStyle.size  = 16.0f;
        riseStyle.bold  = true;
        riseStyle.color = { 1.0f, 0.70f, 0.28f, 1.0f };
        ui.Label({ heatBar.x + heatBar.w + 7.0f, heatBar.y }, rise, riseStyle);
    }
    if (ui.IsHovered(heatBar))
    {
        char tip[160];
        snprintf(tip, sizeof(tip), "Heat: the law's interest in you.\nMissions are %d%% less likely to succeed at this heat.\nIt falls every hour, slower while men are out.",
                 -GetHeatChancePenalty(state.heat));
        ui.Tooltip(tip);
    }
    x += heatBar.w + 40.0f;

    // Evidence, with the raid's threshold marked, then the escapes left.
    const float   evidence    = state.law.evidence;
    const UiRect  evidenceBar = { x, topInset + 13.0f, kEvidenceBarW, 20.0f };
    const bool    pastRaidLine = evidence >= kRaidEvidence;
    ui.ProgressBar(evidenceBar, evidence / kEvidenceMax, pastRaidLine ? Color{ 0.85f, 0.25f, 0.18f, 1.0f } : Color{ 0.72f, 0.58f, 0.30f, 1.0f });
    const float raidLineX = evidenceBar.x + evidenceBar.w * kRaidEvidence / kEvidenceMax;
    ui.Fill({ raidLineX - 1.0f, evidenceBar.y - 3.0f, 2.0f, evidenceBar.h + 6.0f }, { 1.0f, 0.95f, 0.85f, 1.0f });
    char evidenceText[32];
    snprintf(evidenceText, sizeof(evidenceText), "Evidence %d", (int)(evidence + 0.5f));
    ui.LabelIn(evidenceBar, evidenceText, heatStyle);
    if (ui.IsHovered(evidenceBar))
    {
        char tip[360];
        snprintf(tip, sizeof(tip), "Evidence: what investigators have reported.\nA raid comes when it passes the mark (%d) while heat is %d or more.\nCrates count, contraband worst, as do gun racks and money outside a vault.\nShelves in Storage and a Vault help. A Bribe lowers it.\nIt fades slowly while heat is under %d.",
                 (int)kRaidEvidence, (int)kRaidHeat, (int)kEvidenceDecayHeat);
        ui.Tooltip(tip);
    }
    x += evidenceBar.w + 12.0f;

    const int escapes = state.law.EscapesLeft();
    Ui::TextStyle escapesStyle;
    escapesStyle.size  = 15.0f;
    escapesStyle.bold  = true;
    escapesStyle.color = escapes >= 2 ? Color{ 0.75f, 0.85f, 0.65f, 1.0f } : escapes == 1 ? Color{ 1.0f, 0.70f, 0.28f, 1.0f } : Color{ 1.0f, 0.35f, 0.25f, 1.0f };
    char escapesText[32];
    snprintf(escapesText, sizeof(escapesText), "Escapes %d", escapes);
    const UiRect escapesRect = { x, topInset + 13.0f, kEscapesW - 8.0f, 20.0f };
    ui.LabelIn(escapesRect, escapesText, escapesStyle);
    if (ui.IsHovered(escapesRect))
        ui.Tooltip("How many raids the boss can still get away from.\nEach one costs money, crates and half your objects. The one after that catches him.");
    x += kEscapesW;

    // Clock and speeds
    char clockText[32];
    snprintf(clockText, sizeof(clockText), "Day %d  %02d:%02d", state.clock.GetDay(), state.clock.GetHour(), state.clock.GetMinute());
    value.color = { 0.93f, 0.90f, 0.82f, 1.0f };
    ui.Label({ x, textY }, clockText, value);
    x += 136.0f;

    Ui::ButtonStyle pauseStyle;
    pauseStyle.selected = state.clock.IsPaused();
    if (ui.IconButton({ x, buttonY, kButtonSize, kButtonSize }, ui.Texture("ui/icons/pause.tex"), "Pause (Space)", pauseStyle))
        state.clock.SetPaused(!state.clock.IsPaused());
    x += kButtonSize + 4.0f;
    for (int speed = 1; speed <= GameClock::kMaxSpeed; ++speed)
    {
        char label[8];
        snprintf(label, sizeof(label), "%dx", speed);
        Ui::ButtonStyle style;
        style.selected = !state.clock.IsPaused() && state.clock.GetSpeed() == speed;
        if (ui.Button({ x, buttonY, kButtonSize, kButtonSize }, label, style))
        {
            state.clock.SetSpeed(speed);
            state.clock.SetPaused(false);
        }
        x += kButtonSize + 4.0f;
    }

    // View switch, at the right end.
    const bool  narrow  = ui.GetWidth() < kNarrowHudBelow;
    const float buttonW = narrow ? kNarrowViewButtonW : kViewButtonW;
    float viewX = ui.GetWidth() - 14.0f - (buttonW + 4.0f) * (float)std::size(kViewButtons);
    for (const ViewButton& button : kViewButtons)
    {
        const UiRect    r = { viewX, buttonY, buttonW, kButtonSize };
        Ui::ButtonStyle style;
        style.selected = active == button.view;
        if (ui.Button(r, "", style))
            view = button.view;
        ui.Image({ r.x + 8.0f, r.y + 6.0f, 20.0f, 20.0f }, ui.Texture(button.icon));
        if (!narrow)
        {
            Ui::TextStyle text;
            text.bold = style.selected;
            ui.Label({ r.x + 34.0f, r.y + 6.0f }, button.label, text);
        }
        if (ui.IsHovered(r))
            ui.Tooltip("Switch view (Tab)");
        viewX += buttonW + 4.0f;
    }

    m_barBottom = topInset + kBarHeight;
    ui.SetContentTop(m_barBottom);
    return view;
}

void Hud::DrawLawBanner(Ui& ui, const GameState& state) const
{
    const LawSim& law = state.law;
    std::string   text;
    Color         back = { 0.32f, 0.25f, 0.10f, 0.92f };
    if (law.phase == RaidPhase::Warning)
    {
        const int minutes = std::max(0, (int)law.warningMinutes);
        char      line[120];
        snprintf(line, sizeof(line), "RAID in %dh %02dm: ship crates out, send men on missions", minutes / 60, minutes % 60);
        text = line;
        back = { 0.55f + 0.25f * std::sin(m_pulse * 6.0f), 0.08f, 0.06f, 0.95f };
    }
    else if (law.phase == RaidPhase::Police)
    {
        text = law.policeInside ? "RAID: the police are inside" : "RAID: the police are at the door";
        back = { 0.08f, 0.16f, 0.55f + 0.25f * std::sin(m_pulse * 6.0f), 0.95f };
    }
    else if (const int count = law.CountAgents(LawAgentKind::Investigator); count > 0)
    {
        text = count == 1 ? "An investigator is about" : std::to_string(count) + " investigators are about";
    }
    if (text.empty())
        return;
    Ui::TextStyle style;
    style.bold = true;
    const float w = ui.MeasureText(text, style.size, true) + 20.0f;
    const float h = ui.GetLineHeight() + 6.0f;
    const UiRect r = { 0.5f * (ui.GetWidth() - w), m_barBottom + 30.0f, w, h };
    ui.Fill(r, back);
    ui.LabelIn(r, text, style);
}

void Hud::DrawNews(Ui& ui, const GameState& state, float realSeconds)
{
    m_heatWarning = std::max(0.0f, m_heatWarning - realSeconds);
    m_pulse += realSeconds;

    DrawLawBanner(ui, state);

    if (state.events.empty())
        return;

    const GameEvent& event = state.events.back();
    if (event.id != m_shownEvent)
    {
        m_shownEvent   = event.id;
        m_shownSeconds = 0.0f;
    }
    m_shownSeconds += realSeconds;
    if (m_shownSeconds >= kNewsSeconds)
        return;

    Ui::TextStyle news;
    news.color = event.kind == GameEventKind::Good ? Color{ 0.65f, 0.90f, 0.50f, 1.0f }
               : event.kind == GameEventKind::Bad  ? Color{ 0.95f, 0.45f, 0.35f, 1.0f }
                                                   : Color{ 0.93f, 0.90f, 0.82f, 1.0f };
    ui.Fill({ 8.0f, m_barBottom + 4.0f, ui.MeasureText(event.text) + 12.0f, ui.GetLineHeight() + 4.0f }, { 0.05f, 0.05f, 0.05f, 0.7f });
    ui.Label({ 14.0f, m_barBottom + 6.0f }, event.text, news);
}
