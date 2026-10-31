#pragma once

#include "../scene.h"
#include "../ui/ui_painter.h"

#include <cstdint>
#include <vector>

// The city map, a 2D screen: the map itself with its markers and cars, the crew panel on the right,
// the mission popup and the result cards. Rules are in sim/city_sim.h. What's kept here is only
// the view: where the map is looked at, which popup is open and who's picked in it.
class CityScene final : public IScene
{
public:
    explicit CityScene(const SceneServices& services);

    const char* GetName() const override { return "City"; }
    Color       GetClearColor() const override { return { 0.06f, 0.06f, 0.07f, 1.0f }; }
    void        Update(const SceneFrame& frame) override;
    void        Draw(const SceneRenderers&) const override {} // the map is UI

private:
    // The part of the window showing the map, in logical pixels.
    UiRect GetMapArea(const Ui& ui) const;
    void   UpdateCamera(const SceneFrame& frame, const UiRect& area, bool mouseOnMap);
    Vec2   MapToScreen(const UiRect& area, Vec2 map) const;

    void DrawMap(Ui& ui, const UiRect& area);
    void DrawMarkers(Ui& ui, const UiRect& area, bool mouseOnMap);
    void DrawMissions(Ui& ui, const UiRect& area, bool mouseOnMap);
    void DrawCrewPanel(const SceneFrame& frame);
    void DrawPopup(const SceneFrame& frame, const UiRect& area);
    void DrawJobPopup(const SceneFrame& frame, const UiRect& area, const CityMarker& marker);
    void DrawHirePopup(Ui& ui, const UiRect& area, const CityMarker& marker);
    void DrawResultCard(Ui& ui, const UiRect& area);
    void OpenPopup(const CityMarker& marker);
    void TogglePick(const MissionDef& def, uint32_t crewId);

    IWindow&   m_window;
    GameState& m_state;

    Vec2  m_center = { 0.0f, 0.0f }; // map pixels at the middle of the map area
    float m_zoom   = 0.0f;           // logical pixels per map pixel; 0 until first shown

    bool  m_pressedOnMap = false;
    bool  m_dragged      = false;
    Vec2  m_pressAt;
    Vec2  m_lastMouse;

    // Over the map, drawn last frame: a press there isn't on the map, even if the cursor only just
    // got there (Ui's own check for this is a frame late).
    UiRect m_popupRect;
    UiRect m_cardRect;

    uint32_t m_hoveredMarker = 0;
    uint32_t m_popupMarker   = 0; // 0 = none
    std::vector<uint32_t> m_picked;
    int m_popupScroll  = 0; // rows
    int m_rosterScroll = 0;
};
