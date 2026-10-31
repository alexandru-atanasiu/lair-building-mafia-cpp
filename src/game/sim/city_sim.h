#pragma once

struct GameState;
struct SimStep;

// The city map: crew sent out on missions, which earn money and heat.
// A stub for now: add this system's data here and its rules in Tick. GameState holds one of these
// and Simulation::Advance calls Tick on every step of the clock (never while paused).
struct CitySim
{
    void Tick(GameState& state, const SimStep& step);
};
