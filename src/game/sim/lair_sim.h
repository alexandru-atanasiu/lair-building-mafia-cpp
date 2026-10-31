#pragma once

struct GameState;
struct SimStep;

// The lair: digging, rooms, room objects, doors and what the crew do all day. For now Tick only
// has idle crew wander around, to show they walk. Add this system's data here and its rules in
// Tick. GameState holds one of these and Simulation::Advance calls Tick on every step of the clock
// (never while paused).
struct LairSim
{
    bool wanderEnabled = true;

    void Tick(GameState& state, const SimStep& step);
};

// Crates the lair's storage holds beyond the warehouse's racks.
int ExtraCrateCapacity(const GameState& state);
