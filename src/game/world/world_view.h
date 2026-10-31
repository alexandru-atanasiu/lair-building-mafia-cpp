#pragma once

#include "render/mesh_renderer.h"
#include "world_grid.h"

#include <functional>
#include <memory>

class Mesh;
class MeshCache;

// Draws one level of a world grid with placeholder meshes (py/create_meshes.py).
class WorldView
{
public:
    explicit WorldView(MeshCache& meshes);

    // How to draw a wall or door, given the edge as a tile's `side`: `height` is the share of its
    // height, a `disguised` door is the color of the walls, and `doorFilled` is the share of a door
    // that's plain wall instead: at 1 there's no doorway at all.
    struct WallLook
    {
        float height     = 1.0f;
        bool  disguised  = false;
        float doorFilled = 0.0f;
    };
    using WallLooks = std::function<WallLook(const TileCoord& tile, EdgeSide side)>;

    // `wallLooks` is for a view that treats some walls differently; empty draws them all whole.
    void Draw(MeshRenderer::Scope& scope, const WorldGrid& world, WorldLevel level, const WallLooks& wallLooks = {}) const;

    // World matrix of a tile-sized mesh made around the origin (like floor_tile, crate) on a tile.
    static Mat4 TileTransform(const WorldGrid& world, const TileCoord& tile, float yaw = 0.0f);

private:
    std::shared_ptr<const Mesh> m_floor;
    std::shared_ptr<const Mesh> m_earth;
    std::shared_ptr<const Mesh> m_wall;
    std::shared_ptr<const Mesh> m_door;
    std::shared_ptr<const Mesh> m_stairs;
};
