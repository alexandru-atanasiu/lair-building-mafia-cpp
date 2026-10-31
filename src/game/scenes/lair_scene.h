#pragma once

#include "../scene.h"
#include "lair_tools.h"
#include "lair_view.h"
#include "level_view.h"

#include <memory>

// The underground level: the lair. The rules are in sim/lair_sim.h, the drawing and its cutaway in
// lair_view.h, the build menu and the mouse in lair_tools.h.
class LairScene final : public IScene
{
public:
    explicit LairScene(const SceneServices& services);
    ~LairScene() override;

    const char* GetName() const override { return "Lair"; }
    Color       GetClearColor() const override { return { 0.02f, 0.02f, 0.025f, 1.0f }; }
    void        Update(const SceneFrame& frame) override;
    void        Draw(const SceneRenderers& renderers) const override;
    void        OnDeactivate() override { m_tools.CancelDrag(); }
    OrbitCamera* GetOrbitCamera() override { return &m_view.GetCamera(); }
    float       GetTileSurfaceHeight(const TileCoord& tile) const override { return m_lair.GetSurfaceHeight(tile); }

#if defined(GAME_IMGUI_ENABLED)
    void RegisterCheats(DebugPad& pad) override;
#endif

private:
    LevelView                     m_view;
    LairView                      m_lair;
    LairTools                     m_tools;
    std::unique_ptr<MeshRenderer> m_translucent; // marks, blueprints and ghosts
#if defined(GAME_IMGUI_ENABLED)
    DebugPad* m_debugPad = nullptr;
#endif
};
