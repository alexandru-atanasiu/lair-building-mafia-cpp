#pragma once

#include "../world/world_path.h"

#include <vector>

// Where a walking person is and where they're going. Plain simulation data with no rendering:
// anything that walks the world grid holds one (crew, later investigators), and
// CharacterView (character_view.h) draws it. Advance it by simulation time, so it follows pause
// and game speed.
struct CharacterMotion
{
    Vec3       position;
    WorldLevel level  = WorldLevel::Ground;
    float      yaw    = 0.0f;
    float      speed  = 3.2f; // metres per simulated second
    float      radius = 0.3f;

    std::vector<WorldWaypoint> path; // remaining waypoints, empty when standing
    size_t                     nextWaypoint = 0;
    TileCoord                  goal;
    uint64_t                   gridRevision = 0; // the world the path was planned on

    bool IsMoving() const { return nextWaypoint < path.size(); }
};

// Stands on a tile's center, stopped.
void PlaceCharacter(CharacterMotion& motion, const WorldGrid& world, const TileCoord& tile);

// Starts walking to a tile, on any level. False (and stops) if it can't be reached.
bool SendCharacterTo(CharacterMotion& motion, const WorldGrid& world, const TileCoord& goal);

// Walks along the path. Re-plans when the world changed, and stops if the goal became unreachable.
void UpdateCharacterMotion(CharacterMotion& motion, const WorldGrid& world, float simSeconds);
