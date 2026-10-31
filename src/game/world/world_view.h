#pragma once

#include "render/mesh_renderer.h"
#include "world_grid.h"

#include <memory>

class Mesh;
class MeshCache;

// Draws one level of a world grid with placeholder meshes (py/create_meshes.py).
class WorldView
{
public:
    explicit WorldView(MeshCache& meshes);

    void Draw(MeshRenderer::Scope& scope, const WorldGrid& world, WorldLevel level) const;

    // World matrix of a tile-sized mesh made around the origin (like floor_tile, crate) on a tile.
    static Mat4 TileTransform(const WorldGrid& world, const TileCoord& tile, float yaw = 0.0f);

private:
    std::shared_ptr<const Mesh> m_floor;
    std::shared_ptr<const Mesh> m_earth;
    std::shared_ptr<const Mesh> m_wall;
    std::shared_ptr<const Mesh> m_door;
    std::shared_ptr<const Mesh> m_stairs;
};
