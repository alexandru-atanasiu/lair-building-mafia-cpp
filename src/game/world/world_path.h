#pragma once

#include "world_grid.h"

#include <vector>

// Walking on the world grid, across levels through stairs.

// A point to walk to, in order. Consecutive waypoints on different levels are a flight of stairs.
struct WorldWaypoint
{
    Vec3       position;
    WorldLevel level = WorldLevel::Ground;
};

// Shortest tile path (A* over 8-connected tiles and stair links, see WorldGrid::CanStep). `tiles`
// gets the tiles after `from`, ending with `to`; empty if from == to. False if `to` is
// unreachable or not walkable. `from` may be unwalkable (someone just walled in).
bool FindTilePath(const WorldGrid& grid, const TileCoord& from, const TileCoord& to, std::vector<TileCoord>& tiles);

// The tile path as waypoints for a mover of `radius` standing at `position` on tile `from`:
// corners are cut where the way is clear, the last waypoint is the center of `to`.
bool FindWorldPath(const WorldGrid& grid, Vec3 position, const TileCoord& from, const TileCoord& to, float radius,
                   std::vector<WorldWaypoint>& path);

// The segment a-b on `level` and its copies offset by `radius` to either side cross only steps
// allowed by WorldGrid::CanStep.
bool HasClearLine(const WorldGrid& grid, WorldLevel level, Vec3 a, Vec3 b, float radius);

// The walkable tile nearest to `around` on its level (itself if walkable), searching outward ring
// by ring. False if there's none within `maxRings`.
bool FindNearestWalkable(const WorldGrid& grid, const TileCoord& around, int maxRings, TileCoord& result);
