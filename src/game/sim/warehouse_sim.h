#pragma once

struct GameState;
struct SimStep;

// The warehouse on the ground level: the front, and the truck docking with smuggled goods.
// A stub for now: add this system's data here and its rules in Tick. GameState holds one of these
// and Simulation::Advance calls Tick on every step of the clock (never while paused).
struct WarehouseSim
{
    void Tick(GameState& state, const SimStep& step);
};
