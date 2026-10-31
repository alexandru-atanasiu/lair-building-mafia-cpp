#pragma once

#include "../scene.h"
#include "level_view.h"

#include <memory>

class Mesh;

// The ground level: the warehouse, the front. A stub for now: the grid, some crates, and crew who
// happen to come up the stairs.
class WarehouseScene final : public IScene
{
public:
    explicit WarehouseScene(const SceneServices& services);
    ~WarehouseScene() override;

    const char* GetName() const override { return "Warehouse"; }
    Color       GetClearColor() const override { return { 0.05f, 0.05f, 0.06f, 1.0f }; }
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
