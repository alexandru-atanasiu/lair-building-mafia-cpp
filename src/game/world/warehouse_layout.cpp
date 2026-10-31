#include "warehouse_layout.h"

#include <iterator>

namespace
{
    constexpr int   kSlotsAcrossRack = 3;
    constexpr int   kRackLevels      = 2;
    constexpr float kRackSlotSpacing = 1.3f;
    constexpr float kRackLevelY[kRackLevels] = { 0.02f, 1.10f };
    constexpr float kRackTowardWall  = 0.35f; // racks stand back against the wall, past the tile's center

    constexpr int   kOverflowPerLayer = 4; // crates in a layer on one pallet
    constexpr float kOverflowOffset   = 0.48f;
    constexpr float kPalletHeight     = 0.14f;
    constexpr float kCrateHeight      = 0.84f;

    void MarkFixture(WorldGrid& world, const TileCoord& tile)
    {
        world.SetTileKind(tile, TileKind::Fixture);
    }
}

void LayOutWarehouse(WorldGrid& world)
{
    for (const WarehouseRack& rack : kWarehouseRacks)
    {
        MarkFixture(world, { WorldLevel::Ground, rack.x, rack.z });
        MarkFixture(world, { WorldLevel::Ground, rack.x + 1, rack.z });
    }
    for (const WarehouseRack& rack : kStockRacksAlongX)
    {
        MarkFixture(world, { WorldLevel::Ground, rack.x, rack.z });
        MarkFixture(world, { WorldLevel::Ground, rack.x + 1, rack.z });
    }
    for (const WarehouseRack& rack : kStockRacksAlongZ)
    {
        MarkFixture(world, { WorldLevel::Ground, rack.x, rack.z });
        MarkFixture(world, { WorldLevel::Ground, rack.x, rack.z + 1 });
    }
    for (const TileCoord& tile : kOverflowTiles)
        MarkFixture(world, tile);
    for (const TileCoord& tile : kGoodsTiles)
        MarkFixture(world, tile);
    for (const TileCoord& tile : kForkliftTiles)
        MarkFixture(world, tile);
    MarkFixture(world, kDeskTile);
    MarkFixture(world, { WorldLevel::Ground, kDeskTile.x + 1, kDeskTile.z });
    MarkFixture(world, kCabinetTile);

    for (int z = kOfficeZ0; z <= kWarehouseZ1; ++z)
        world.SetEdge({ WorldLevel::Ground, kOfficeX1, z }, EdgeSide::PosX, EdgeKind::Wall);
    for (int x = kWarehouseX0; x <= kOfficeX1; ++x)
        world.SetEdge({ WorldLevel::Ground, x, kOfficeZ0 }, EdgeSide::NegZ, EdgeKind::Wall);
    world.SetEdge({ WorldLevel::Ground, kOfficeX1, kOfficeZ0 + 1 }, EdgeSide::PosX, EdgeKind::Door);
}

int GetRackSlotCount()
{
    return (int)std::size(kWarehouseRacks) * kSlotsAcrossRack * kRackLevels;
}

Vec3 GetRackPosition(const WorldGrid& world, const WarehouseRack& rack)
{
    return world.TileCenter({ WorldLevel::Ground, rack.x, rack.z }) + Vec3{ 0.5f * kTileSize, 0.0f, kRackTowardWall };
}

CrateSlot GetCrateSlot(const WorldGrid& world, int slot)
{
    CrateSlot result;
    if (slot < GetRackSlotCount())
    {
        const int perRack = kSlotsAcrossRack * kRackLevels;
        const WarehouseRack& rack = kWarehouseRacks[slot / perRack];
        const int inRack = slot % perRack;
        const int across = inRack % kSlotsAcrossRack;
        const int level  = inRack / kSlotsAcrossRack;
        const Vec3 base  = GetRackPosition(world, rack);

        result.position = base + Vec3{ ((float)across - 1.0f) * kRackSlotSpacing, kRackLevelY[level], 0.0f };
        result.tile     = { WorldLevel::Ground, rack.x + (across > 0 ? 1 : 0), rack.z };
        result.stand    = { WorldLevel::Ground, result.tile.x, rack.z - 1 };
        return result;
    }

    // Stacks on the overflow pallets: fill a layer across all of them, then the next layer up.
    const int  index     = slot - GetRackSlotCount();
    const int  tileCount = (int)std::size(kOverflowTiles);
    const int  perLayer  = tileCount * kOverflowPerLayer;
    const int  layer     = index / perLayer;
    const int  inLayer   = index % perLayer;
    const TileCoord& tile = kOverflowTiles[inLayer / kOverflowPerLayer];
    const int  corner    = inLayer % kOverflowPerLayer;

    result.position = world.TileCenter(tile) + Vec3{ (corner & 1) ? kOverflowOffset : -kOverflowOffset,
                                                     kPalletHeight + (float)layer * kCrateHeight,
                                                     (corner & 2) ? kOverflowOffset : -kOverflowOffset };
    result.tile  = tile;
    result.stand = { WorldLevel::Ground, tile.x, tile.z + 1 };
    return result;
}
