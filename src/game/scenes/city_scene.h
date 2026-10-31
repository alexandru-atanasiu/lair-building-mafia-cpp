#pragma once

#include "../scene.h"

// The city map, a 2D screen. A stub for now: the placeholder map, the crew list and one of each
// game UI widget, to show them off. Missions go here (their rules in sim/city_sim.h).
class CityScene final : public IScene
{
public:
    explicit CityScene(const SceneServices& services);

    const char* GetName() const override { return "City"; }
    Color       GetClearColor() const override { return { 0.06f, 0.06f, 0.07f, 1.0f }; }
    void        Update(const SceneFrame& frame) override;
    void        Draw(const SceneRenderers&) const override {} // the map is UI

private:
    GameState& m_state;
    int        m_clicks = 0;
};
