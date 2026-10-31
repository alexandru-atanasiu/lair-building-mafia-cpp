#pragma once

#include "../scene.h"

class IWindow;

// The thin bar along the top of every view: money, heat, the clock with its speed controls, the
// view switch and the latest news. Space pauses, 1/2/3 set the speed, Tab goes to the next view.
class Hud
{
public:
    // Builds this frame's bar and takes its keys. `topInset`: logical pixels to leave free above
    // it (the dev menu bar). Returns the view to show: `active`, or another one the player asked for.
    ViewId Update(Ui& ui, GameState& state, const IWindow& window, bool keyboardAvailable, ViewId active, float topInset);

    // The newest news line, under the bar for a few seconds. Call after the scene's UI: it goes on
    // top of it.
    void DrawNews(Ui& ui, const GameState& state, float realSeconds);

private:
    float    m_barBottom    = 0.0f;
    uint64_t m_shownEvent   = 0; // id of the news line on show
    float    m_shownSeconds = 0.0f;
    float    m_lastHeat     = 0.0f; // the heat bar warns when it jumps
    float    m_heatRise     = 0.0f; // how much it rose, while the warning shows
    float    m_heatWarning  = 0.0f; // real seconds left of the warning
};
