#pragma once

#include "../scene.h"
#include "level_view.h"

#include <memory>

class Mesh;

// The underground level: the lair. A stub for now: the grid, some crates and crew wandering
// (lair_sim.cpp). Right click sends the first crew member to a tile, to try the pathing.
class LairScene final : public IScene
{
public:
    explicit LairScene(const SceneServices& services);
    ~LairScene() override;

    const char* GetName() const override { return "Lair"; }
    Color       GetClearColor() const override { return { 0.02f, 0.02f, 0.025f, 1.0f }; }
    void        Update(const SceneFrame& frame) override;
    void        Draw(const SceneRenderers& renderers) const override;
    OrbitCamera* GetOrbitCamera() override { return &m_view.GetCamera(); }

#if defined(GAME_IMGUI_ENABLED)
    void RegisterCheats(DebugPad& pad) override;
#endif

private:
    GameState&                  m_state;
    LevelView                   m_view;
    std::shared_ptr<const Mesh> m_crate;
#if defined(GAME_IMGUI_ENABLED)
    DebugPad* m_debugPad = nullptr;
#endif
};
