#include "world_view.h"

#include "core/fatal.h"
#include "render/mesh.h"
#include "render/mesh_cache.h"

#include <cmath>

namespace
{
    constexpr const char* kFloorMesh  = "meshes/world/floor_tile.mesh";
    constexpr const char* kEarthMesh  = "meshes/world/earth_block.mesh";
    constexpr const char* kWallMesh   = "meshes/world/wall_segment.mesh";
    constexpr const char* kDoorMesh   = "meshes/world/door_frame.mesh";
    constexpr const char* kStairsMesh = "meshes/world/stairs.mesh";

    const Color kFloorColors[2]    = { { 0.42f, 0.38f, 0.33f, 1.0f }, { 0.38f, 0.35f, 0.30f, 1.0f } };
    const Color kPavementColors[2] = { { 0.27f, 0.28f, 0.30f, 1.0f }, { 0.25f, 0.26f, 0.28f, 1.0f } };
    const Color kEarthColors[2]    = { { 0.24f, 0.18f, 0.13f, 1.0f }, { 0.21f, 0.16f, 0.12f, 1.0f } };
    const Color kWallColor         = { 0.62f, 0.56f, 0.48f, 1.0f };
    const Color kDoorColor         = { 0.40f, 0.25f, 0.14f, 1.0f };
    const Color kStairsColor       = { 0.50f, 0.47f, 0.42f, 1.0f };
}

WorldView::WorldView(MeshCache& meshes)
    : m_floor(LoadOrFatal(meshes.Load(kFloorMesh), kFloorMesh))
    , m_earth(LoadOrFatal(meshes.Load(kEarthMesh), kEarthMesh))
    , m_wall(LoadOrFatal(meshes.Load(kWallMesh), kWallMesh))
    , m_door(LoadOrFatal(meshes.Load(kDoorMesh), kDoorMesh))
    , m_stairs(LoadOrFatal(meshes.Load(kStairsMesh), kStairsMesh))
{
}

Mat4 WorldView::TileTransform(const WorldGrid& world, const TileCoord& tile, float yaw)
{
    return Mat4::Translation(world.TileCenter(tile)) * Mat4::RotationY(yaw);
}

void WorldView::Draw(MeshRenderer::Scope& scope, const WorldGrid& world, WorldLevel level, const WallLooks& wallLooks) const
{
    const int width = world.GetWidth();
    const int depth = world.GetDepth();

    for (int z = 0; z < depth; ++z)
    {
        for (int x = 0; x < width; ++x)
        {
            const TileCoord c       = { level, x, z };
            const Mat4      at      = Mat4::Translation(world.TileCenter(c));
            const int       checker = (x + z) & 1;
            switch (world.GetTile(c).kind)
            {
            case TileKind::Earth:     scope.Draw(*m_earth, at, kEarthColors[checker]); break;
            case TileKind::Floor:     scope.Draw(*m_floor, at, kFloorColors[checker]); break;
            case TileKind::Pavement:  scope.Draw(*m_floor, at, kPavementColors[checker]); break;
            case TileKind::Stairwell: break; // the stairs fill it (a hole in the ground level)
            case TileKind::Fixture:   scope.Draw(*m_floor, at, kFloorColors[checker]); break;
            }

            // Each edge once: this tile's far sides, and the near sides on the grid's border.
            const auto drawEdge = [&](EdgeSide side, Vec3 offset, float yaw) {
                const EdgeKind edge = world.GetEdge(c, side);
                if (edge == EdgeKind::None)
                    return;
                const WallLook look = wallLooks ? wallLooks(c, side) : WallLook{};
                const Mat4 place = Mat4::Translation(world.TileCenter(c) + offset) * Mat4::RotationY(yaw);
                if (edge == EdgeKind::Wall)
                {
                    scope.Draw(*m_wall, place * Mat4::Scale({ 1.0f, look.height, 1.0f }), kWallColor);
                    return;
                }
                // The door below, the wall closing it above: together, always the whole height.
                const float doorHeight = look.height * (1.0f - look.doorFilled);
                if (doorHeight > 0.0f)
                    scope.Draw(*m_door, place * Mat4::Scale({ 1.0f, doorHeight, 1.0f }), look.disguised ? kWallColor : kDoorColor);
                if (look.doorFilled > 0.0f)
                    scope.Draw(*m_wall, place * Mat4::Translation({ 0.0f, doorHeight * kWallHeight, 0.0f }) * Mat4::Scale({ 1.0f, look.height * look.doorFilled, 1.0f }), kWallColor);
            };
            const float half = 0.5f * kTileSize;
            drawEdge(EdgeSide::PosX, { half, 0.0f, 0.0f }, 0.5f * kPi);
            drawEdge(EdgeSide::PosZ, { 0.0f, 0.0f, half }, 0.0f);
            if (x == 0)
                drawEdge(EdgeSide::NegX, { -half, 0.0f, 0.0f }, 0.5f * kPi);
            if (z == 0)
                drawEdge(EdgeSide::NegZ, { 0.0f, 0.0f, -half }, 0.0f);
        }
    }

    // The mesh goes down along +Z from the upper tile; both levels see the same stairs.
    for (const Stair& stair : world.GetStairs())
    {
        const float yaw = std::atan2((float)(stair.lower.x - stair.upper.x), (float)(stair.lower.z - stair.upper.z));
        scope.Draw(*m_stairs, Mat4::Translation(world.TileCenter(stair.upper)) * Mat4::RotationY(yaw), kStairsColor);
    }
}
