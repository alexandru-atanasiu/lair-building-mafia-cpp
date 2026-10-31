#pragma once

#include "world_grid.h"

// The place at the start of a game: a walled warehouse on the pavement, stairs down from its back
// room, and a small dug-out basement at their foot. Change it here.
constexpr int kWorldWidth = 40;
constexpr int kWorldDepth = 40;

// The one way between the levels, going down along +Z. The top is in the back room
// (warehouse_layout.h). The starting crew in game_state.cpp and the bolt-hole in law_sim.cpp stand
// in the basement around the foot.
constexpr TileCoord kStairTop  = { WorldLevel::Ground, 25, 21 };
constexpr TileCoord kStairFoot = { WorldLevel::Underground, kStairTop.x, kStairTop.z + kStairSpanTiles };

WorldGrid MakeStartingWorld();

// Tiles the views start looking at.
TileCoord GetWarehouseCenter();
TileCoord GetBasementCenter();
