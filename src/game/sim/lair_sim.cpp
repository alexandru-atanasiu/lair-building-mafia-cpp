#include "lair_sim.h"

#include "game_state.h"

#include <vector>

namespace
{
    constexpr float kMinIdleSeconds = 1.5f;
    constexpr float kMaxIdleSeconds = 6.0f;
    constexpr int   kGroundLevelChance = 5; // one walk in this many crosses to the other level

    // A random Floor tile of the level.
    bool PickFloorTile(GameState& state, WorldLevel level, TileCoord& tile)
    {
        std::vector<TileCoord> floors;
        for (int z = 0; z < state.world.GetDepth(); ++z)
            for (int x = 0; x < state.world.GetWidth(); ++x)
                if (state.world.GetTile({ level, x, z }).kind == TileKind::Floor)
                    floors.push_back({ level, x, z });
        if (floors.empty())
            return false;
        tile = floors[state.random() % floors.size()];
        return true;
    }
}

// Stub until lair-building's storage objects are merged: delete it then (theirs is in lair_build.cpp).
int ExtraCrateCapacity(const GameState&)
{
    return 0;
}

void LairSim::Tick(GameState& state, const SimStep& step)
{
    if (!wanderEnabled)
        return;

    for (CrewMember& member : state.crew)
    {
        if (member.away || member.jobId != 0 || member.motion.IsMoving())
            continue;

        member.idleTime -= step.simSeconds;
        if (member.idleTime > 0.0f)
            continue;

        const float t = (float)(state.random() % 1000) / 1000.0f;
        member.idleTime = kMinIdleSeconds + t * (kMaxIdleSeconds - kMinIdleSeconds);

        WorldLevel level = member.motion.level;
        // The boss stays below.
        if (member.role != CrewRole::Boss && state.random() % kGroundLevelChance == 0)
            level = level == WorldLevel::Ground ? WorldLevel::Underground : WorldLevel::Ground;

        TileCoord goal;
        if (PickFloorTile(state, level, goal))
            SendCharacterTo(member.motion, state.world, goal);
    }
}
