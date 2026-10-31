#include "city_scene.h"

#include "../ui/ui.h"
#include "platform/window.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>

namespace
{
    constexpr const char* kMapTexture = "ui/city/map.tex";

    constexpr float kPanelWidth    = 304.0f; // the crew panel, on the right
    constexpr float kRosterRow     = 58.0f;
    constexpr float kRosterHeader  = 40.0f;
    constexpr float kMaxPhysicalZoom = 2.0f; // screen pixels per map pixel: the art gets soft past this
    constexpr float kStartZoom     = 0.85f;
    constexpr float kWheelZoomStep = 1.14f;
    constexpr float kPanSpeed      = 800.0f; // logical pixels a second
    constexpr float kDragThreshold = 4.0f;   // logical pixels before a press is a drag, not a click

    constexpr float kMarkerSize   = 40.0f;
    constexpr float kCarPlate     = 30.0f;
    constexpr float kExpiringSoon = 0.25f; // fraction of its life left: the marker's bar turns orange

    constexpr float kPopupWidth   = 480.0f;
    constexpr float kPopupRow     = 40.0f;

    const Color kDim      = { 0.72f, 0.69f, 0.62f, 1.0f };
    const Color kGood     = { 0.65f, 0.90f, 0.50f, 1.0f };
    const Color kBad      = { 0.98f, 0.62f, 0.34f, 1.0f };
    const Color kAmber    = { 0.92f, 0.76f, 0.38f, 1.0f };
    const Color kPipOn    = { 0.92f, 0.82f, 0.56f, 1.0f };
    const Color kPipOff   = { 0.28f, 0.26f, 0.23f, 1.0f };
    const Color kPipTest  = { 0.98f, 0.90f, 0.50f, 1.0f };

    // Crops a map-layer rectangle to the map area: the HUD and the crew panel are on top of it,
    // but the HUD was drawn first.
    void ImageInArea(Ui& ui, const UiRect& area, UiRect r, const ITexture& texture, const Color& color)
    {
        UvRect uv;
        const float top = std::max(r.y, area.y);
        if (top >= r.y + r.h)
            return;
        uv.v0 = (top - r.y) / r.h;
        r.h  -= top - r.y;
        r.y   = top;
        ui.Image(r, texture, color, uv);
    }

    void DrawPortrait(Ui& ui, const UiRect& r, const Color& tint)
    {
        ui.Fill(r, { 0.74f, 0.71f, 0.62f, 1.0f });
        ui.Image(r, ui.Texture("ui/city/portrait_suit.tex"), tint);
        ui.Image(r, ui.Texture("ui/city/portrait_head.tex"));
        ui.Frame(r, 1.0f, { 0.15f, 0.13f, 0.10f, 1.0f });
    }

    // "M" and five pips: returns the width taken.
    float DrawStatPips(Ui& ui, Vec2 p, const char* label, int value, bool highlight)
    {
        Ui::TextStyle text;
        text.size  = 13.0f;
        text.color = highlight ? kPipTest : kDim;
        text.bold  = highlight;
        ui.Label(p, label, text);
        const float labelWidth = ui.MeasureText(label, 13.0f, highlight) + 4.0f;
        for (int i = 0; i < 5; ++i)
            ui.Fill({ p.x + labelWidth + (float)i * 9.0f, p.y + 4.0f, 7.0f, 8.0f }, i < value ? (highlight ? kPipTest : kPipOn) : kPipOff);
        return labelWidth + 45.0f;
    }

    std::string MoneyText(int amount)
    {
        std::string digits = std::to_string(amount);
        for (int i = (int)digits.size() - 3; i > 0; i -= 3)
            digits.insert((size_t)i, ",");
        return "$" + digits;
    }

    std::string HoursText(float hours)
    {
        const int whole = std::max(1, (int)std::ceil(hours));
        return std::to_string(whole) + (whole == 1 ? " hour" : " hours");
    }

    std::string Signed(int value)
    {
        return (value >= 0 ? "+" : "") + std::to_string(value);
    }

    Ui::PanelStyle PopupStyle()
    {
        Ui::PanelStyle style;
        style.fill = { 0.10f, 0.09f, 0.08f, 0.985f };
        return style;
    }

    // The texture for a heading: car_0 faces up, then clockwise by 45 degrees.
    const char* CarTexture(Vec2 direction)
    {
        static const char* const kCars[] = {
            "ui/city/car_0.tex", "ui/city/car_1.tex", "ui/city/car_2.tex", "ui/city/car_3.tex",
            "ui/city/car_4.tex", "ui/city/car_5.tex", "ui/city/car_6.tex", "ui/city/car_7.tex",
        };
        const float angle = std::atan2(direction.x, -direction.y); // clockwise from up
        const int   step  = ((int)std::lround(angle / (kPi / 4.0f)) + 8) % 8;
        return kCars[step];
    }

    const char* PhaseText(MissionPhase phase)
    {
        switch (phase)
        {
        case MissionPhase::Leaving: return "Heading out";
        case MissionPhase::DrivingOut: return "Driving there";
        case MissionPhase::Working: return "On the job";
        case MissionPhase::DrivingBack: return "Driving back";
        }
        return "";
    }
}

CityScene::CityScene(const SceneServices& services)
    : m_window(services.window)
    , m_state(services.state)
{
    m_center = { kCityWarehouse.x + 500.0f, kCityWarehouse.y - 300.0f };
}

UiRect CityScene::GetMapArea(const Ui& ui) const
{
    const float top = ui.GetContentTop();
    return { 0.0f, top, std::max(ui.GetWidth() - kPanelWidth, 1.0f), std::max(ui.GetHeight() - top, 1.0f) };
}

Vec2 CityScene::MapToScreen(const UiRect& area, Vec2 map) const
{
    return { area.x + 0.5f * area.w + (map.x - m_center.x) * m_zoom, area.y + 0.5f * area.h + (map.y - m_center.y) * m_zoom };
}

void CityScene::UpdateCamera(const SceneFrame& frame, const UiRect& area, bool mouseOnMap)
{
    Ui& ui = frame.ui;

    // Never stretched, and always covering the area: a wider or narrower window sees more or less
    // of the city, at one scale both ways.
    const float minZoom = std::max(area.w / kCityMapWidth, area.h / kCityMapHeight);
    const float maxZoom = std::max(minZoom, kMaxPhysicalZoom / ui.GetScale());
    if (m_zoom <= 0.0f)
        m_zoom = kStartZoom;

    const Vec2 mouse = ui.GetMouse();
    if (mouseOnMap)
    {
        const float wheel = m_window.GetMouseWheelDelta();
        if (wheel != 0.0f)
        {
            // The point under the cursor stays there.
            const float before = m_zoom;
            m_zoom = std::clamp(m_zoom * std::pow(kWheelZoomStep, wheel), minZoom, maxZoom);
            const Vec2 fromCenter = { mouse.x - (area.x + 0.5f * area.w), mouse.y - (area.y + 0.5f * area.h) };
            m_center.x += fromCenter.x / before - fromCenter.x / m_zoom;
            m_center.y += fromCenter.y / before - fromCenter.y / m_zoom;
        }
    }
    m_zoom = std::clamp(m_zoom, minZoom, maxZoom);

    if (frame.keyboardAvailable)
    {
        const float step = kPanSpeed * frame.realSeconds / m_zoom;
        if (m_window.IsKeyDown(Key::A) || m_window.IsKeyDown(Key::Left))
            m_center.x -= step;
        if (m_window.IsKeyDown(Key::D) || m_window.IsKeyDown(Key::Right))
            m_center.x += step;
        if (m_window.IsKeyDown(Key::W) || m_window.IsKeyDown(Key::Up))
            m_center.y -= step;
        if (m_window.IsKeyDown(Key::S) || m_window.IsKeyDown(Key::Down))
            m_center.y += step;
    }

    // A press on the map drags it, with either button, or is a click when it hardly moves.
    const bool down = m_window.IsMouseButtonDown(MouseButton::Left) || m_window.IsMouseButtonDown(MouseButton::Right);
    if (mouseOnMap && (m_window.WasMouseButtonPressed(MouseButton::Left) || m_window.WasMouseButtonPressed(MouseButton::Right)))
    {
        m_pressedOnMap = true;
        m_dragged      = false;
        m_pressAt      = mouse;
        m_lastMouse    = mouse;
    }
    if (m_pressedOnMap && down)
    {
        if (std::abs(mouse.x - m_pressAt.x) > kDragThreshold || std::abs(mouse.y - m_pressAt.y) > kDragThreshold)
            m_dragged = true;
        if (m_dragged)
        {
            m_center.x -= (mouse.x - m_lastMouse.x) / m_zoom;
            m_center.y -= (mouse.y - m_lastMouse.y) / m_zoom;
        }
        m_lastMouse = mouse;
    }

    const float halfW = 0.5f * area.w / m_zoom;
    const float halfH = 0.5f * area.h / m_zoom;
    m_center.x = std::clamp(m_center.x, halfW, std::max(halfW, kCityMapWidth - halfW));
    m_center.y = std::clamp(m_center.y, halfH, std::max(halfH, kCityMapHeight - halfH));

    // For the dev driver: map pixels to physical pixels.
    const Vec2 origin = MapToScreen(area, { 0.0f, 0.0f });
    m_state.city.screenScale   = m_zoom * ui.GetScale();
    m_state.city.screenOffsetX = origin.x * ui.GetScale();
    m_state.city.screenOffsetY = origin.y * ui.GetScale();
}

void CityScene::DrawMap(Ui& ui, const UiRect& area)
{
    const float halfW = 0.5f * area.w / m_zoom;
    const float halfH = 0.5f * area.h / m_zoom;
    UvRect      uv;
    uv.u0 = (m_center.x - halfW) / kCityMapWidth;
    uv.u1 = (m_center.x + halfW) / kCityMapWidth;
    uv.v0 = (m_center.y - halfH) / kCityMapHeight;
    uv.v1 = (m_center.y + halfH) / kCityMapHeight;
    ui.Image(area, ui.Texture(kMapTexture), {}, uv);

    // District names, under everything else on the map.
    Ui::TextStyle style;
    style.size  = 15.0f;
    style.bold  = true;
    style.align = Ui::Align::Center;
    style.color = { 0.95f, 0.92f, 0.82f, 0.85f };
    for (const CityDistrict& district : kCityDistricts)
    {
        const Vec2 p = MapToScreen(area, { district.x, district.y });
        if (p.y < area.y + 4.0f || p.x < 0.0f || p.x > area.x + area.w)
            continue;
        std::string name = district.name;
        for (char& c : name)
            c = (char)std::toupper((unsigned char)c);
        const float width = ui.MeasureText(name, style.size, true) + 14.0f;
        ui.Fill({ p.x - 0.5f * width, p.y - 12.0f, width, 24.0f }, { 0.08f, 0.07f, 0.06f, 0.55f });
        ui.Label({ p.x, p.y - 10.0f }, name, style);
    }

    // The warehouse: fixed, and not a mission.
    const Vec2   home = MapToScreen(area, { kCityWarehouse.x, kCityWarehouse.y });
    const float  size = kMarkerSize + 6.0f;
    const UiRect disc = { home.x - 0.5f * size, home.y - 0.5f * size, size, size };
    ImageInArea(ui, area, disc, ui.Texture("ui/city/plate.tex"), {});
    ImageInArea(ui, area, { disc.x + 9.0f, disc.y + 9.0f, size - 18.0f, size - 18.0f }, ui.Texture("ui/icons/warehouse.tex"), { 0.16f, 0.14f, 0.12f, 1.0f });
    Ui::TextStyle label;
    label.size  = 14.0f;
    label.bold  = true;
    label.align = Ui::Align::Center;
    if (home.y + 0.5f * size > area.y)
        ui.Label({ home.x, home.y + 0.5f * size + 1.0f }, kCityWarehouse.name, label);
}

void CityScene::DrawMarkers(Ui& ui, const UiRect& area, bool mouseOnMap)
{
    const auto   defs  = GetMissionDefs();
    const Vec2   mouse = ui.GetMouse();
    const double now   = m_state.clock.GetTotalMinutes();

    m_hoveredMarker = 0;
    float bestDistance = 0.5f * kMarkerSize + 2.0f;
    if (mouseOnMap)
    {
        for (const CityMarker& marker : m_state.city.markers)
        {
            const Vec2  p = MapToScreen(area, { kCitySites[marker.site].x, kCitySites[marker.site].y });
            const float d = std::hypot(p.x - mouse.x, p.y - mouse.y);
            if (d < bestDistance)
            {
                bestDistance    = d;
                m_hoveredMarker = marker.id;
            }
        }
    }

    for (const CityMarker& marker : m_state.city.markers)
    {
        const MissionDef& def     = defs[(size_t)marker.def];
        const CitySite&   site    = kCitySites[marker.site];
        const Vec2        p       = MapToScreen(area, { site.x, site.y });
        const bool        hovered = marker.id == m_hoveredMarker;
        const bool        open    = marker.id == m_popupMarker;
        const float       size    = hovered || open ? kMarkerSize + 6.0f : kMarkerSize;
        const UiRect      r       = { p.x - 0.5f * size, p.y - 0.5f * size, size, size };

        ImageInArea(ui, area, r, ui.Texture("ui/city/disc.tex"), open ? Color{ 1.0f, 0.95f, 0.70f, 1.0f } : def.color);
        const float inset = size * 0.24f;
        ImageInArea(ui, area, { r.x + inset, r.y + inset, size - 2.0f * inset, size - 2.0f * inset }, ui.Texture(def.icon), def.color);

        // How long it will wait.
        const float left = (float)std::clamp((marker.expiresAt - now) / (double)marker.lifeMinutes, 0.0, 1.0);
        if (r.y + size > area.y)
        {
            const UiRect back = { p.x - 16.0f, r.y + size + 1.0f, 32.0f, 4.0f };
            ui.Fill(back, { 0.05f, 0.05f, 0.05f, 0.85f });
            ui.Fill({ back.x + 1.0f, back.y + 1.0f, 30.0f * left, 2.0f }, left < kExpiringSoon ? kBad : kPipOn);
        }

        if (hovered)
        {
            char tip[160];
            if (def.effect == MissionEffect::Hire)
                snprintf(tip, sizeof(tip), "%s at %s\nGone in %s", def.name, site.name, HoursText((float)(marker.expiresAt - now) / 60.0f).c_str());
            else
                snprintf(tip, sizeof(tip), "%s at %s\nReward %s, gone in %s", def.name, site.name, MoneyText(def.reward).c_str(), HoursText((float)(marker.expiresAt - now) / 60.0f).c_str());
            ui.Tooltip(tip);
        }
    }
}

void CityScene::DrawMissions(Ui& ui, const UiRect& area, bool mouseOnMap)
{
    const auto defs  = GetMissionDefs();
    const Vec2 mouse = ui.GetMouse();

    for (const ActiveMission& mission : m_state.city.missions)
    {
        const MissionDef& def  = defs[(size_t)mission.def];
        const CitySite&   site = kCitySites[mission.site];
        std::string       names;
        for (uint32_t id : mission.crew)
        {
            if (const CrewMember* member = m_state.FindCrew(id))
                names += (names.empty() ? "" : ", ") + member->name;
        }

        // The place of the job: its marker, dimmed, with the job's progress.
        const Vec2   at = MapToScreen(area, { site.x, site.y });
        const UiRect r  = { at.x - 0.5f * kMarkerSize, at.y - 0.5f * kMarkerSize, kMarkerSize, kMarkerSize };
        const Color  dim = { def.color.r * 0.6f, def.color.g * 0.6f, def.color.b * 0.6f, 1.0f };
        ImageInArea(ui, area, r, ui.Texture("ui/city/disc.tex"), dim);
        ImageInArea(ui, area, { r.x + 10.0f, r.y + 10.0f, 20.0f, 20.0f }, ui.Texture(def.icon), dim);
        if (mission.phase == MissionPhase::Working && r.y + kMarkerSize > area.y)
        {
            const float  fraction = 1.0f - mission.workMinutesLeft / std::max(def.hours * 60.0f, 1.0f);
            const UiRect back     = { at.x - 16.0f, r.y + kMarkerSize + 1.0f, 32.0f, 4.0f };
            ui.Fill(back, { 0.05f, 0.05f, 0.05f, 0.85f });
            ui.Fill({ back.x + 1.0f, back.y + 1.0f, 30.0f * std::clamp(fraction, 0.0f, 1.0f), 2.0f }, kGood);
        }

        // The car, on the road.
        Vec2 car, direction;
        mission.SampleCar(car, direction);
        const Vec2 carAt      = MapToScreen(area, car);
        const bool onTheRoad  = mission.phase == MissionPhase::DrivingOut || mission.phase == MissionPhase::DrivingBack;
        if (onTheRoad)
        {
            const CrewMember* lead  = mission.crew.empty() ? nullptr : m_state.FindCrew(mission.crew.front());
            const Color       tint  = lead ? lead->tint : Color{ 0.4f, 0.4f, 0.4f, 1.0f };
            const UiRect      plate = { carAt.x - 0.5f * kCarPlate, carAt.y - 0.5f * kCarPlate, kCarPlate, kCarPlate };
            ImageInArea(ui, area, plate, ui.Texture("ui/city/plate.tex"), {});
            ImageInArea(ui, area, { plate.x + 3.0f, plate.y + 3.0f, kCarPlate - 6.0f, kCarPlate - 6.0f }, ui.Texture(CarTexture(direction)), tint);
            Ui::TextStyle label;
            label.size  = 13.0f;
            label.bold  = true;
            label.align = Ui::Align::Center;
            if (plate.y + kCarPlate > area.y)
                ui.Label({ carAt.x, plate.y + kCarPlate }, names, label);
        }

        if (mouseOnMap)
        {
            const bool overPlace = std::hypot(at.x - mouse.x, at.y - mouse.y) < 0.5f * kMarkerSize;
            const bool overCar   = onTheRoad && std::hypot(carAt.x - mouse.x, carAt.y - mouse.y) < 0.5f * kCarPlate;
            if (overPlace || overCar)
            {
                char tip[200];
                if (mission.phase == MissionPhase::Working)
                    snprintf(tip, sizeof(tip), "%s at %s\n%s: on the job, %s left", def.name, site.name, names.c_str(), HoursText(mission.workMinutesLeft / 60.0f).c_str());
                else
                    snprintf(tip, sizeof(tip), "%s at %s\n%s: %s", def.name, site.name, names.c_str(), PhaseText(mission.phase));
                ui.Tooltip(tip);
            }
        }
    }
}

void CityScene::DrawCrewPanel(const SceneFrame& frame)
{
    Ui&          ui   = frame.ui;
    const float  top  = ui.GetContentTop();
    const UiRect rect = { ui.GetWidth() - kPanelWidth, top - 1.0f, kPanelWidth + 2.0f, ui.GetHeight() - top + 3.0f };
    ui.Panel(rect);

    const float x = rect.x + 12.0f;
    float       y = top + 8.0f;

    ui.Image({ x, y + 2.0f, 22.0f, 22.0f }, ui.Texture("ui/icons/crew.tex"));
    Ui::TextStyle title;
    title.size = 20.0f;
    title.bold = true;
    ui.Label({ x + 28.0f, y }, "The crew", title);
    y = top + kRosterHeader;

    const int count   = (int)m_state.crew.size();
    const int visible = std::max(1, (int)((ui.GetHeight() - y - 4.0f) / kRosterRow));
    const int maxTop  = std::max(0, count - visible);
    if (rect.Contains(ui.GetMouse()) && frame.mouseAvailable)
        m_rosterScroll -= (int)m_window.GetMouseWheelDelta();
    m_rosterScroll = std::clamp(m_rosterScroll, 0, maxTop);

    for (int i = m_rosterScroll; i < std::min(count, m_rosterScroll + visible); ++i)
    {
        const CrewMember& member = m_state.crew[(size_t)i];
        const UiRect      row    = { x - 4.0f, y, kPanelWidth - 18.0f, kRosterRow - 4.0f };
        ui.Fill(row, { 1.0f, 1.0f, 1.0f, 0.035f });
        DrawPortrait(ui, { x, y + 4.0f, 46.0f, 46.0f }, member.tint);

        const float textX = x + 54.0f;
        Ui::TextStyle name;
        name.bold = true;
        ui.Label({ textX, y + 5.0f }, member.name, name);

        Ui::TextStyle status;
        status.size  = 14.0f;
        status.align = Ui::Align::Right;
        std::string statusText;
        std::string tip;
        switch (m_state.city.GetCrewStatus(member))
        {
        case CrewStatus::Boss:
            statusText   = "The boss";
            status.color = kDim;
            tip          = "The boss stays home. He doesn't go on missions.";
            break;
        case CrewStatus::Free:
            statusText   = "Free";
            status.color = kGood;
            break;
        case CrewStatus::Working:
            statusText   = "Working";
            status.color = kAmber;
            tip          = "Sending him on a mission takes him off his job.";
            break;
        case CrewStatus::OnMission:
        {
            const ActiveMission* mission = m_state.city.FindMissionOf(member.id);
            statusText   = mission ? PhaseText(mission->phase) : "On a mission";
            status.color = { 0.62f, 0.78f, 0.92f, 1.0f };
            if (mission)
                tip = std::string("On a mission: ") + GetMissionDefs()[(size_t)mission->def].name + " at " + kCitySites[mission->site].name + ".";
            break;
        }
        case CrewStatus::Wounded:
        {
            const CrewOut* entry = m_state.city.FindOut(member.id);
            statusText   = "Wounded, " + std::to_string((int)std::ceil((entry ? entry->minutesLeft : 0.0f) / 60.0f)) + " h";
            status.color = kBad;
            tip          = "Wounded: back when he's patched up.";
            break;
        }
        case CrewStatus::Arrested:
        {
            const CrewOut* entry = m_state.city.FindOut(member.id);
            statusText   = "Arrested, " + std::to_string((int)std::ceil((entry ? entry->minutesLeft : 0.0f) / 60.0f)) + " h";
            status.color = kBad;
            tip          = "Arrested: back when he makes bail.";
            break;
        }
        }
        ui.Label({ row.x + row.w - 6.0f, y + 6.0f }, statusText, status);

        float pipX = textX;
        pipX += DrawStatPips(ui, { pipX, y + 29.0f }, "M", member.muscle, false) + 9.0f;
        pipX += DrawStatPips(ui, { pipX, y + 29.0f }, "B", member.brains, false) + 9.0f;
        DrawStatPips(ui, { pipX, y + 29.0f }, "N", member.nerve, false);

        if (ui.IsHovered(row))
        {
            char text[200];
            snprintf(text, sizeof(text), "%s\nMuscle %d, Brains %d, Nerve %d%s%s", member.name.c_str(), member.muscle, member.brains, member.nerve, tip.empty() ? "" : "\n", tip.c_str());
            ui.Tooltip(text);
        }
        y += kRosterRow;
    }

    if (maxTop > 0)
    {
        const float trackY = top + kRosterHeader;
        const float trackH = (float)visible * kRosterRow - 4.0f;
        ui.Fill({ rect.x + kPanelWidth - 6.0f, trackY, 3.0f, trackH }, { 1.0f, 1.0f, 1.0f, 0.08f });
        ui.Fill({ rect.x + kPanelWidth - 6.0f, trackY + trackH * (float)m_rosterScroll / (float)count, 3.0f, trackH * (float)visible / (float)count }, kAmber);
    }
}

void CityScene::OpenPopup(const CityMarker& marker)
{
    m_popupMarker = marker.id;
    m_popupScroll = 0;
    m_picked      = m_state.city.DefaultPick(m_state, GetMissionDefs()[(size_t)marker.def]);
}

void CityScene::TogglePick(const MissionDef& def, uint32_t crewId)
{
    const auto found = std::find(m_picked.begin(), m_picked.end(), crewId);
    if (found != m_picked.end())
    {
        m_picked.erase(found);
        return;
    }
    // When full, the new man replaces the one picked longest ago.
    if ((int)m_picked.size() >= def.maxCrew)
        m_picked.erase(m_picked.begin());
    m_picked.push_back(crewId);
}

void CityScene::DrawPopup(const SceneFrame& frame, const UiRect& area)
{
    const CityMarker* marker = m_state.city.FindMarker(m_popupMarker);
    if (!marker)
    {
        m_popupMarker = 0; // it expired, or was taken
        return;
    }
    if (GetMissionDefs()[(size_t)marker->def].effect == MissionEffect::Hire)
        DrawHirePopup(frame.ui, area, *marker);
    else
        DrawJobPopup(frame, area, *marker);
}

void CityScene::DrawJobPopup(const SceneFrame& frame, const UiRect& area, const CityMarker& marker)
{
    Ui&               ui   = frame.ui;
    const MissionDef& def  = GetMissionDefs()[(size_t)marker.def];
    const CitySite&   site = kCitySites[marker.site];
    const CitySim&    city = m_state.city;

    // Men who left since (hurt, or sent elsewhere) come off the picks.
    std::erase_if(m_picked, [this](uint32_t id) {
        const CrewMember* member = m_state.FindCrew(id);
        return !member || !m_state.city.IsAvailable(m_state, *member);
    });

    std::vector<const CrewMember*> candidates;
    for (const CrewMember& member : m_state.crew)
        if (city.IsAvailable(m_state, member))
            candidates.push_back(&member);
    std::stable_sort(candidates.begin(), candidates.end(), [&def](const CrewMember* a, const CrewMember* b) {
        if ((a->jobId != 0) != (b->jobId != 0))
            return a->jobId == 0;
        return GetCrewStat(*a, def.stat) > GetCrewStat(*b, def.stat);
    });

    // As many rows as fit: everything else has a fixed height.
    const float fixedHeight = 334.0f;
    const float available   = area.h - 16.0f;
    const int   rowsThatFit = std::max(1, (int)((available - fixedHeight) / kPopupRow));
    const int   rows        = std::clamp((int)candidates.size(), 1, rowsThatFit);
    const float height      = fixedHeight + (float)rows * kPopupRow;

    const UiRect r = { area.x + std::max(8.0f, 0.5f * (area.w - kPopupWidth)), area.y + std::max(8.0f, 0.5f * (area.h - height)), kPopupWidth, height };
    ui.Panel(r, PopupStyle());
    m_popupRect = r;

    const float x = r.x + 16.0f;
    const float w = r.w - 32.0f;
    float       y = r.y + 12.0f;

    // Title
    ui.Image({ x, y, 42.0f, 42.0f }, ui.Texture("ui/city/disc.tex"), def.color);
    ui.Image({ x + 10.0f, y + 10.0f, 22.0f, 22.0f }, ui.Texture(def.icon), def.color);
    Ui::TextStyle title;
    title.size = 22.0f;
    title.bold = true;
    ui.Label({ x + 52.0f, y - 2.0f }, def.name, title);
    Ui::TextStyle where;
    where.size  = 14.0f;
    where.color = kDim;
    ui.Label({ x + 52.0f, y + 25.0f }, site.name, where);
    y += 50.0f;

    Ui::TextStyle blurb;
    blurb.size  = 14.0f;
    blurb.color = kDim;
    ui.Label({ x, y }, ui.WrapText(def.blurb, w, blurb.size), blurb);
    y += 42.0f;

    // The terms, two columns.
    const double now = m_state.clock.GetTotalMinutes();
    char men[32];
    if (def.minCrew == def.maxCrew)
        snprintf(men, sizeof(men), "%d", def.minCrew);
    else
        snprintf(men, sizeof(men), "%d to %d", def.minCrew, def.maxCrew);
    const std::string reward = def.reward > 0 ? MoneyText(def.reward) : "None";
    const std::string heat   = Signed((int)std::lround(def.heat));
    const std::string fail   = "Lose it all, heat " + Signed((int)std::lround(def.failHeat));
    const struct { const char* label; std::string value; Color color; } terms[] = {
        { "Investment", MoneyText(def.investment), def.investment > 0 ? kAmber : kGood },
        { "Reward", reward, kGood },
        { "Takes", HoursText(def.hours) + " + the drive", kPipOn },
        { "Heat", heat, def.heat > 0.0f ? kBad : kGood },
        { "Men", men, kPipOn },
        { "Tests", GetCrewStatName(def.stat), kPipTest },
        { "Gone in", HoursText((float)(marker.expiresAt - now) / 60.0f), kPipOn },
        { "If it fails", fail, kBad },
    };
    for (int i = 0; i < (int)std::size(terms); ++i)
    {
        const float col    = i % 2 == 0 ? x : x + 0.5f * w;
        const float rowY   = y + (float)(i / 2) * 21.0f;
        Ui::TextStyle label;
        label.size  = 14.0f;
        label.color = kDim;
        ui.Label({ col, rowY }, terms[i].label, label);
        Ui::TextStyle value;
        value.size  = 14.0f;
        value.bold  = true;
        value.color = terms[i].color;
        ui.Label({ col + 76.0f, rowY }, terms[i].value, value);
    }
    y += 88.0f;

    // The chance, for the men picked.
    const MissionChance chance = city.ComputeChance(m_state, def, m_picked);
    const float         t      = (float)chance.total / 100.0f;
    const Color         barColor = { 0.85f - 0.45f * t, 0.42f + 0.28f * t, 0.22f, 1.0f };
    char chanceText[64];
    if (m_picked.empty())
        snprintf(chanceText, sizeof(chanceText), "Pick who goes");
    else
        snprintf(chanceText, sizeof(chanceText), "Chance of success: %d%%", chance.total);
    const UiRect bar = { x, y, w, 26.0f };
    ui.ProgressBar(bar, m_picked.empty() ? 0.0f : t, barColor);
    Ui::TextStyle chanceStyle;
    chanceStyle.bold = true;
    ui.LabelIn(bar, chanceText, chanceStyle);
    y += 29.0f;

    int worstNerve = 5;
    for (uint32_t id : m_picked)
        if (const CrewMember* member = m_state.FindCrew(id))
            worstNerve = std::min(worstNerve, member->nerve);
    std::string parts = "Base " + std::to_string(chance.base);
    if (!m_picked.empty())
        parts += std::string(", ") + GetCrewStatName(def.stat) + " " + Signed(chance.stat);
    if (chance.extraMen != 0)
        parts += ", extra men " + Signed(chance.extraMen);
    if (chance.heat != 0)
        parts += ", heat " + Signed(chance.heat);
    parts += ".";
    if (!m_picked.empty())
        parts += " If it fails: wounded " + std::to_string(CitySim::WoundChance(worstNerve)) + "%, arrested " + std::to_string(CitySim::ArrestChance(worstNerve)) + "%.";
    Ui::TextStyle partsStyle;
    partsStyle.size  = 13.0f;
    partsStyle.color = kDim;
    ui.Label({ x, y }, ui.WrapText(parts, w, partsStyle.size), partsStyle);
    y += 36.0f;

    // Who goes
    char pick[48];
    snprintf(pick, sizeof(pick), def.maxCrew == 1 ? "Pick the man (%d of %d)" : "Pick the men (%d of %d)", (int)m_picked.size(), def.maxCrew);
    Ui::TextStyle pickStyle;
    pickStyle.bold = true;
    ui.Label({ x, y }, pick, pickStyle);
    y += 23.0f;

    const float listTop = y;
    const int   maxTop  = std::max(0, (int)candidates.size() - rows);
    const UiRect list   = { x, y, w, (float)rows * kPopupRow };
    if (list.Contains(ui.GetMouse()) && frame.mouseAvailable)
        m_popupScroll -= (int)m_window.GetMouseWheelDelta();
    m_popupScroll = std::clamp(m_popupScroll, 0, maxTop);

    if (candidates.empty())
    {
        Ui::TextStyle nobody;
        nobody.color = kDim;
        ui.LabelIn(list, "Nobody is available.", nobody);
    }
    for (int i = m_popupScroll; i < std::min((int)candidates.size(), m_popupScroll + rows); ++i)
    {
        const CrewMember& member = *candidates[(size_t)i];
        const UiRect      row    = { x, y, w - (maxTop > 0 ? 8.0f : 0.0f), kPopupRow - 3.0f };
        Ui::ButtonStyle   style;
        style.selected = std::find(m_picked.begin(), m_picked.end(), member.id) != m_picked.end();
        if (ui.Button(row, "", style))
            TogglePick(def, member.id);
        DrawPortrait(ui, { row.x + 4.0f, row.y + 3.0f, 31.0f, 31.0f }, member.tint);
        Ui::TextStyle name;
        name.bold = true;
        ui.Label({ row.x + 42.0f, row.y + 9.0f }, member.name, name);
        Ui::TextStyle status;
        status.size  = 13.0f;
        status.color = member.jobId != 0 ? kAmber : kGood;
        ui.Label({ row.x + 122.0f, row.y + 11.0f }, member.jobId != 0 ? "Working" : "Free", status);

        // The stat tested and Nerve, which decides how a failure ends.
        float pipX = row.x + row.w - 214.0f;
        const CrewStat stats[] = { CrewStat::Muscle, CrewStat::Brains, CrewStat::Nerve };
        const char*    names[] = { "M", "B", "N" };
        for (int s = 0; s < 3; ++s)
            pipX += DrawStatPips(ui, { pipX, row.y + 11.0f }, names[s], GetCrewStat(member, stats[s]), stats[s] == def.stat) + 6.0f;
        y += kPopupRow;
    }
    y = listTop + (float)rows * kPopupRow;
    if (maxTop > 0)
    {
        ui.Fill({ x + w - 4.0f, listTop, 3.0f, list.h }, { 1.0f, 1.0f, 1.0f, 0.08f });
        const float total = (float)candidates.size();
        ui.Fill({ x + w - 4.0f, listTop + list.h * (float)m_popupScroll / total, 3.0f, list.h * (float)rows / total }, kAmber);
    }
    y += 4.0f;

    // Go
    const char*     why = city.WhyNotGo(m_state, def, m_picked);
    Ui::ButtonStyle goStyle;
    goStyle.disabled = why != nullptr;
    goStyle.textSize = 18.0f;
    const float buttonW = 150.0f;
    if (ui.Button({ x + w - buttonW, y, buttonW, 36.0f }, "Go", goStyle))
    {
        m_state.city.StartMission(m_state, marker.id, m_picked);
        m_popupMarker = 0;
        return;
    }
    if (ui.Button({ x + w - 2.0f * buttonW - 8.0f, y, buttonW, 36.0f }, "Not now"))
    {
        m_popupMarker = 0;
        return;
    }
    if (why)
    {
        Ui::TextStyle reason;
        reason.size  = 14.0f;
        reason.color = kBad;
        ui.Label({ x, y + 9.0f }, why, reason);
    }
}

void CityScene::DrawHirePopup(Ui& ui, const UiRect& area, const CityMarker& marker)
{
    const MissionDef&       def  = GetMissionDefs()[(size_t)marker.def];
    const CitySite&         site = kCitySites[marker.site];
    const RecruitCandidate& man  = marker.candidate;
    const double            now  = m_state.clock.GetTotalMinutes();

    const float  height = 250.0f;
    const float  width  = 400.0f;
    const UiRect r = { area.x + std::max(8.0f, 0.5f * (area.w - width)), area.y + std::max(8.0f, 0.5f * (area.h - height)), width, height };
    ui.Panel(r, PopupStyle());
    m_popupRect = r;

    const float x = r.x + 16.0f;
    const float w = r.w - 32.0f;
    float       y = r.y + 12.0f;

    ui.Image({ x, y, 42.0f, 42.0f }, ui.Texture("ui/city/disc.tex"), def.color);
    ui.Image({ x + 10.0f, y + 10.0f, 22.0f, 22.0f }, ui.Texture(def.icon), def.color);
    Ui::TextStyle title;
    title.size = 22.0f;
    title.bold = true;
    ui.Label({ x + 52.0f, y - 2.0f }, def.name, title);
    Ui::TextStyle where;
    where.size  = 14.0f;
    where.color = kDim;
    char whereText[96];
    snprintf(whereText, sizeof(whereText), "%s. Gone in %s", site.name, HoursText((float)(marker.expiresAt - now) / 60.0f).c_str());
    ui.Label({ x + 52.0f, y + 25.0f }, whereText, where);
    y += 54.0f;

    DrawPortrait(ui, { x, y, 76.0f, 76.0f }, man.tint);
    Ui::TextStyle name;
    name.size = 20.0f;
    name.bold = true;
    ui.Label({ x + 88.0f, y - 1.0f }, man.name, name);
    float pipX = x + 88.0f;
    pipX += DrawStatPips(ui, { pipX, y + 30.0f }, "Muscle", man.muscle, false) + 12.0f;
    DrawStatPips(ui, { pipX, y + 30.0f }, "Brains", man.brains, false);
    DrawStatPips(ui, { x + 88.0f, y + 52.0f }, "Nerve", man.nerve, false);
    y += 88.0f;

    Ui::TextStyle line;
    line.size  = 14.0f;
    line.color = kDim;
    char terms[96];
    snprintf(terms, sizeof(terms), "He costs %s to bring in. The lair has %d men in %d bunks.", MoneyText(def.investment).c_str(), (int)m_state.crew.size(), GetCrewCapacity(m_state));
    ui.Label({ x, y }, ui.WrapText(terms, w, line.size), line);
    y += 44.0f;

    const char*     why = m_state.city.WhyNotHire(m_state, def);
    Ui::ButtonStyle hire;
    hire.disabled = why != nullptr;
    hire.textSize = 18.0f;
    const float buttonW = 110.0f;
    if (ui.Button({ x + w - buttonW, y, buttonW, 36.0f }, "Hire", hire))
    {
        m_state.city.HireCandidate(m_state, marker.id);
        m_popupMarker = 0;
        return;
    }
    if (ui.Button({ x + w - 2.0f * buttonW - 8.0f, y, buttonW, 36.0f }, "Not now"))
    {
        m_popupMarker = 0;
        return;
    }
    if (why)
    {
        Ui::TextStyle reason;
        reason.size  = 14.0f;
        reason.color = kBad;
        ui.Label({ x, y + 9.0f }, why, reason);
    }
}

void CityScene::DrawResultCard(Ui& ui, const UiRect& area)
{
    std::vector<MissionResult>& results = m_state.city.results;
    if (results.empty())
        return;
    const MissionResult& result = results.front();

    const float       width = std::min(420.0f, area.w - 24.0f);
    const std::string text  = ui.WrapText(result.text, width - 28.0f, 14.0f);
    const long long   lines = 1 + std::count(text.begin(), text.end(), '\n');
    const float       height = 84.0f + (float)lines * ui.GetLineHeight(14.0f);
    const UiRect      r = { area.x + 12.0f, area.y + area.h - height - 12.0f, width, height };
    ui.Panel(r);
    m_cardRect = r;

    Ui::TextStyle title;
    title.size  = 18.0f;
    title.bold  = true;
    title.color = result.success ? kGood : kBad;
    ui.Label({ r.x + 14.0f, r.y + 10.0f }, result.title, title);
    Ui::TextStyle body;
    body.size = 14.0f;
    ui.Label({ r.x + 14.0f, r.y + 36.0f }, text, body);

    if (results.size() > 1)
    {
        char more[32];
        snprintf(more, sizeof(more), "%d more", (int)results.size() - 1);
        Ui::TextStyle moreStyle;
        moreStyle.size  = 14.0f;
        moreStyle.color = kDim;
        ui.Label({ r.x + 14.0f, r.y + r.h - 32.0f }, more, moreStyle);
    }
    if (ui.Button({ r.x + r.w - 92.0f, r.y + r.h - 38.0f, 80.0f, 30.0f }, "OK"))
        results.erase(results.begin());
}

void CityScene::Update(const SceneFrame& frame)
{
    Ui&          ui   = frame.ui;
    const UiRect area = GetMapArea(ui);

    // The area leaves out the HUD and the crew panel; what else covers the map is known by its rects.
    const Vec2 mouse      = ui.GetMouse();
    const bool mouseOnMap = frame.mouseAvailable && area.Contains(mouse) && !m_popupRect.Contains(mouse) && !m_cardRect.Contains(mouse);
    m_popupRect = {};
    m_cardRect  = {};

    UpdateCamera(frame, area, mouseOnMap);
    DrawMap(ui, area);
    DrawMarkers(ui, area, mouseOnMap && !m_dragged);
    DrawMissions(ui, area, mouseOnMap && !m_dragged);

    // A click: a press on the map that didn't turn into a drag.
    const bool leftUp  = m_window.WasMouseButtonReleased(MouseButton::Left);
    const bool rightUp = m_window.WasMouseButtonReleased(MouseButton::Right);
    if (leftUp || rightUp)
    {
        if (m_pressedOnMap && !m_dragged)
        {
            if (rightUp)
            {
                m_popupMarker = 0;
            }
            else if (const CityMarker* marker = m_state.city.FindMarker(m_hoveredMarker))
            {
                if (marker->id != m_popupMarker)
                    OpenPopup(*marker);
            }
            else
            {
                m_popupMarker = 0;
            }
        }
        m_pressedOnMap = false;
        m_dragged      = false;
    }
    if (frame.keyboardAvailable && m_window.WasKeyPressed(Key::Escape))
        m_popupMarker = 0;

    DrawCrewPanel(frame);
    DrawResultCard(ui, area);
    DrawPopup(frame, area);
}
