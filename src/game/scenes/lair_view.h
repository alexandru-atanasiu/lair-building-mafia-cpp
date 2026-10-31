#pragma once

#include "../scene.h"
#include "render/mesh_renderer.h"

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

class Camera3D;
class Mesh;

// Draws the lair as a cutaway. Earth and walls are as tall as they can be without hiding the floor
// from this camera: from each one, look along the line of sight away from the camera, and find the
// first dug tile behind it. Anything on the line to that tile's floor must stay under the line.
// So walls and rock facing the camera sink to a curb in front of a room, the far ones stand at
// full height, and it all follows the camera as it turns. Heights ease toward their targets.
class LairView
{
public:
    explicit LairView(const SceneServices& services);

    // Heights and doors. Call after the camera moved.
    void Update(const Camera3D& camera, float realSeconds, float simSeconds);

    // The opaque world: earth, floors, walls, doors, stairs, objects.
    void Draw(MeshRenderer::Scope& meshes) const;
    // What's marked or being built, over the world and crew, in a translucent scope.
    void DrawOverlays(MeshRenderer::Scope& translucent) const;

    // Height of what's shown on a tile above the floor: earth's top, or 0.
    float GetSurfaceHeight(const TileCoord& tile) const;
    // Height of the wall shown on an edge.
    float GetEdgeHeight(const LairEdge& edge) const;
    // The tile a ray hits first: a block of earth at its shown height, or else the floor. False
    // outside the grid.
    bool PickTile(Vec3 origin, Vec3 direction, TileCoord& tile) const;

    // Pieces for the tools' ghosts.
    void DrawTileOverlay(MeshRenderer::Scope& scope, const TileCoord& tile, const Color& color) const;
    void DrawWallGhost(MeshRenderer::Scope& scope, const LairEdge& edge, float height, const Color& color) const;
    void DrawDoorGhost(MeshRenderer::Scope& scope, const LairEdge& edge, const Color& color) const;
    // `tint`'s alpha is used for all parts; with tinted set, its color too.
    void DrawObjectGhost(MeshRenderer::Scope& scope, int type, const TileCoord& tile, int rotation, const Color& tint, bool tinted, float grow = 1.0f) const;

private:
    size_t TileIndex(int x, int z) const { return (size_t)z * (size_t)m_width + (size_t)x; }
    size_t EdgeXIndex(int x, int z) const { return (size_t)z * (size_t)(m_width + 1) + (size_t)(x + 1); }
    size_t EdgeZIndex(int x, int z) const { return (size_t)(z + 1) * (size_t)m_width + (size_t)x; }

    bool  IsOpen(const TileCoord& tile) const;
    // How tall something at `from` may be: walks the line of sight away from the camera.
    float AllowedHeight(Vec3 from, Vec3 across, const TileCoord* self) const;
    void  ComputeTargets();
    void  DrawEarth(MeshRenderer::Scope& meshes, const TileCoord& tile) const;
    void  DrawWall(MeshRenderer::Scope& meshes, const LairEdge& edge, float height, const Color& color) const;
    Mat4  EdgeTransform(const LairEdge& edge) const;

    struct ObjectParts
    {
        std::vector<std::pair<std::shared_ptr<const Mesh>, Color>> parts;
    };

    GameState& m_state;
    int        m_width;
    int        m_depth;

    std::shared_ptr<const Mesh> m_floor;
    std::shared_ptr<const Mesh> m_earth[3];
    std::shared_ptr<const Mesh> m_wall;
    std::shared_ptr<const Mesh> m_doorFrame;
    std::shared_ptr<const Mesh> m_doorLeaf;
    std::shared_ptr<const Mesh> m_stairs;
    std::shared_ptr<const Mesh> m_chip;
    std::vector<ObjectParts>    m_objectParts; // by object type

    Vec3     m_cameraPosition;
    uint64_t m_targetsRevision = 0;
    float    m_time = 0.0f;
    std::vector<float> m_earthHeight, m_earthTarget;
    std::vector<float> m_edgeXHeight, m_edgeXTarget;
    std::vector<float> m_edgeZHeight, m_edgeZTarget;
    std::unordered_map<int, float> m_doorAngles; // by door id: signed radians, 0 closed
};
