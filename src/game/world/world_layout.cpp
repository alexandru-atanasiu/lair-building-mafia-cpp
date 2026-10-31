#include "world_layout.h"

#include "warehouse_layout.h"

namespace
{
    struct TileRect
    {
        int x0, z0, x1, z1; // inclusive
    };
    constexpr TileRect kWarehouse  = { 12, 10, 27, 23 };
    constexpr TileRect kBasement   = { kStairFoot.x - 2, kStairFoot.z, kStairFoot.x + 2, kStairFoot.z + 4 }; // the rest is earth to dig

    void FillRect(WorldGrid& world, WorldLevel level, const TileRect& r, TileKind kind)
    {
        for (int z = r.z0; z <= r.z1; ++z)
            for (int x = r.x0; x <= r.x1; ++x)
                world.SetTileKind({ level, x, z }, kind);
    }

    void WallAround(WorldGrid& world, WorldLevel level, const TileRect& r)
    {
        for (int x = r.x0; x <= r.x1; ++x)
        {
            world.SetEdge({ level, x, r.z0 }, EdgeSide::NegZ, EdgeKind::Wall);
            world.SetEdge({ level, x, r.z1 }, EdgeSide::PosZ, EdgeKind::Wall);
        }
        for (int z = r.z0; z <= r.z1; ++z)
        {
            world.SetEdge({ level, r.x0, z }, EdgeSide::NegX, EdgeKind::Wall);
            world.SetEdge({ level, r.x1, z }, EdgeSide::PosX, EdgeKind::Wall);
        }
    }
}

WorldGrid MakeStartingWorld()
{
    WorldGrid world(kWorldWidth, kWorldDepth);

    FillRect(world, WorldLevel::Ground, { 0, 0, kWorldWidth - 1, kWorldDepth - 1 }, TileKind::Pavement);
    FillRect(world, WorldLevel::Ground, kWarehouse, TileKind::Floor);
    WallAround(world, WorldLevel::Ground, kWarehouse);
    world.SetEdge({ WorldLevel::Ground, 19, kWarehouse.z0 }, EdgeSide::NegZ, EdgeKind::Door); // the front door

    FillRect(world, WorldLevel::Underground, kBasement, TileKind::Floor);

    world.AddStair(kStairTop, kStairFoot);
    LayOutWarehouse(world);
    return world;
}

TileCoord GetWarehouseCenter()
{
    return { WorldLevel::Ground, 20, 14 };
}

TileCoord GetBasementCenter()
{
    return { WorldLevel::Underground, kStairFoot.x, kStairFoot.z + 2 };
}
