#pragma once

struct GameState;
struct SimStep;

// The law: high heat brings investigators, then raids; two escapes, busted on the third.
// A stub for now: add this system's data here and its rules in Tick. GameState holds one of these
// and Simulation::Advance calls Tick on every step of the clock (never while paused).
struct LawSim
{
    void Tick(GameState& state, const SimStep& step);
};
