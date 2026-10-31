#pragma once

#include "world_grid.h"

// The place at the start of a game: a walled warehouse on the pavement, stairs down, and a small
// dug-out basement of two rooms joined by a door. Change it here.
constexpr int kWorldWidth = 40;
constexpr int kWorldDepth = 40;

WorldGrid MakeStartingWorld();

// Tiles the views start looking at.
TileCoord GetWarehouseCenter();
TileCoord GetBasementCenter();
