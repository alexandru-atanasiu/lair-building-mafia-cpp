#pragma once

#include "game_state.h"

// Runs the game forward by the clock. Owned by Game; every view shares it, and it keeps running
// whichever view is on screen.
class Simulation
{
public:
    Simulation() = default;

    Simulation(const Simulation&)            = delete;
    Simulation& operator=(const Simulation&) = delete;

    // Advances the clock and every system. Returns the step taken (0 while paused).
    SimStep Advance(float realSeconds);

    GameState&       GetState() { return m_state; }
    const GameState& GetState() const { return m_state; }

private:
    GameState m_state;
};
