#pragma once

#include "world_grid.h"

// The warehouse's fixed floor plan: where its racks, pallets and office are. The player doesn't
// build here. world_layout.cpp calls LayOutWarehouse, warehouse_sim uses the crate slots and the
// dock, warehouse_scene draws it all.
//
// Seen from the default camera (+X right, +Z away): the front door in the near wall, racks along the
// far wall, the office in the far left corner, the back room in the far right corner, and the big
// door in the right wall, onto the yard and the road.
//
// The back room holds the stairs down. A row of shelves stands in front of it, leaving a one-tile
// passage between them and the room's wall, open only at the far end from the door: you walk in
// behind the shelves from the racks' side. It's all ordinary walls, a door and fixtures.

// The building's tiles, inclusive; keep in sync with kWarehouse in world_layout.cpp.
constexpr int kWarehouseX0 = 12;
constexpr int kWarehouseZ0 = 10;
constexpr int kWarehouseX1 = 27;
constexpr int kWarehouseZ1 = 23;

// The big door: on the PosX edge of these two tiles of the building's right wall. A wall while
// shut, nothing while open (WarehouseSim changes the edges).
constexpr TileCoord kDockDoorTiles[2] = { { WorldLevel::Ground, kWarehouseX1, 17 }, { WorldLevel::Ground, kWarehouseX1, 18 } };
// Where crew stand to reach the back of a docked truck: just outside the door.
constexpr TileCoord kTruckStandTiles[2] = { { WorldLevel::Ground, kWarehouseX1 + 1, 17 }, { WorldLevel::Ground, kWarehouseX1 + 1, 18 } };
// Where carriers go when they're done, inside the door.
constexpr TileCoord kDockStagingTile = { WorldLevel::Ground, kWarehouseX1 - 3, 18 };

// Fixtures that stop people walking, each marked TileKind::Fixture. `sizeX` x 1 tiles.
struct WarehouseRack
{
    int x, z; // the rack's lowest tile; it covers x and x + 1
};
constexpr WarehouseRack kWarehouseRacks[] = {
    { 16, kWarehouseZ1 }, { 18, kWarehouseZ1 }, { 20, kWarehouseZ1 }, { 22, kWarehouseZ1 },
};
// Racks of the front's honest stock, full and never used for crates. The second list runs along Z
// (they cover z and z + 1).
constexpr WarehouseRack kStockRacksAlongX[] = { { 22, kWarehouseZ0 }, { 24, kWarehouseZ0 }, { 16, 18 }, { 18, 18 } };
constexpr WarehouseRack kStockRacksAlongZ[] = { { kWarehouseX0, 16 }, { kWarehouseX0, 18 } };
// The back room, up to the building's corner, and where its door is: on this tile's NegZ edge.
constexpr int       kBackRoomX0   = 24;
constexpr int       kBackRoomZ0   = 21;
constexpr TileCoord kBackRoomDoor = { WorldLevel::Ground, 26, kBackRoomZ0 };
// The shelves in front of it, full of old crates that are never counted, and the passage behind them.
constexpr WarehouseRack kHidingShelves[] = { { 24, kBackRoomZ0 - 2 }, { 26, kBackRoomZ0 - 2 } };
// Pallets that take the crates beyond the racks' capacity (ExtraCrateCapacity).
constexpr TileCoord kOverflowTiles[] = {
    { WorldLevel::Ground, 23, 12 }, { WorldLevel::Ground, 24, 12 }, { WorldLevel::Ground, 25, 12 }, { WorldLevel::Ground, 26, 12 },
};
// Dressing: pallets of the front's honest goods, parked forklift (two tiles, along X), office.
constexpr TileCoord kGoodsTiles[] = {
    { WorldLevel::Ground, 13, 12 }, { WorldLevel::Ground, 14, 12 }, { WorldLevel::Ground, 15, 12 },
    { WorldLevel::Ground, 13, 14 }, { WorldLevel::Ground, 14, 14 },
    { WorldLevel::Ground, 13, 16 },
    { WorldLevel::Ground, 20, 16 }, { WorldLevel::Ground, 21, 16 }, { WorldLevel::Ground, 20, 15 },
};
constexpr TileCoord kForkliftTiles[2] = { { WorldLevel::Ground, 25, 15 }, { WorldLevel::Ground, 26, 15 } };
constexpr TileCoord kDeskTile      = { WorldLevel::Ground, 13, 22 };
constexpr TileCoord kCabinetTile   = { WorldLevel::Ground, 12, 23 };

// The office: walls on two sides of this corner, a door in one.
constexpr int kOfficeX1 = 15;
constexpr int kOfficeZ0 = 20;

// Marks the fixtures and builds the office and the back room. The building's walls and floor are
// already there.
void LayOutWarehouse(WorldGrid& world);

// A crate's place. Slots 0 .. GetRackSlotCount() - 1 are on the racks; the slots after them are
// stacks on the overflow pallets, as many as anyone asks for.
struct CrateSlot
{
    Vec3      position; // the crate's bottom center
    float     yaw = 0.0f;
    TileCoord tile;     // the tile it's on (a fixture)
    TileCoord stand;    // the walkable tile a carrier reaches it from
};
int       GetRackSlotCount();
CrateSlot GetCrateSlot(const WorldGrid& world, int slot);

// Middle of a rack, on the floor (the mesh's origin).
Vec3 GetRackPosition(const WorldGrid& world, const WarehouseRack& rack);
