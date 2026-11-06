#pragma once

#include "../scene.h"
#include "render/mesh_renderer.h"

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

class Mesh;

// Draws the lair. Earth and walls are shown as they are, whatever the camera does: standing at full
// height, or gone once dug out or deleted. Only work in progress changes a height (rock being dug,
// walls going up).
class LairView
{
public:
    explicit LairView(const SceneServices& services);

    // Doors and the digging's rock chips.
    void Update(float simSeconds);

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

    float    m_time = 0.0f;
    std::unordered_map<int, float> m_doorAngles; // by door id: signed radians, 0 closed
};
