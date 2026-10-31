#pragma once

#include "../camera/orbit_camera.h"
#include "../scene.h"
#include "render/lighting.h"
#include "render/mesh_renderer.h"
#include "render/skinned_mesh_renderer.h"

// What the two 3D scenes share: a camera over one level of the world, its lighting, the grid and
// the crew standing on that level. Scene-specific meshes go in through `drawExtras`.
class LevelView
{
public:
    LevelView(const SceneServices& services, WorldLevel level, const TileCoord& startFocus, const MeshLighting& lighting);

    // Camera and crew animation. Call after the scene built its UI: `frame.ui` decides who gets
    // the mouse.
    void Update(const SceneFrame& frame);

    // Opens a mesh scope with the level drawn in it, calls `drawExtras(scope)`, then draws the crew.
    template <typename DrawExtras>
    void Draw(const SceneRenderers& renderers, DrawExtras&& drawExtras) const
    {
        {
            MeshRenderer::Scope meshes = renderers.meshes.Begin(m_camera.GetCamera(), m_lighting);
            DrawLevel(meshes);
            drawExtras(meshes);
        }
        DrawCrew(renderers);
    }

    // The crew on this level, in their own skinned scope.
    void DrawCrew(const SceneRenderers& renderers) const;

    MeshLighting&       GetLighting() { return m_lighting; }
    const MeshLighting& GetLighting() const { return m_lighting; }
    OrbitCamera&       GetCamera() { return m_camera; }
    const OrbitCamera& GetCamera() const { return m_camera; }
    WorldLevel         GetLevel() const { return m_level; }

    // The walkable tile under the cursor, if any. Hit-tests the level's floor.
    bool TileUnderCursor(TileCoord& tile) const;

private:
    void DrawLevel(MeshRenderer::Scope& meshes) const;
    bool IsShownHere(const CrewMember& member) const;

    const SceneServices m_services;
    WorldLevel          m_level;
    OrbitCamera         m_camera;
    MeshLighting        m_lighting;
};
