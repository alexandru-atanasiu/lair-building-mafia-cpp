#include "warehouse_scene.h"

#include "../world/world_layout.h"
#include "../world/world_view.h"
#include "core/fatal.h"
#include "render/mesh.h"
#include "render/mesh_cache.h"

#if defined(GAME_IMGUI_ENABLED)
#include "debug/debug_pad.h"
#endif

namespace
{
    constexpr const char* kCrateMesh = "meshes/props/crate.mesh";

    const Color kCrateColor = { 0.55f, 0.40f, 0.22f, 1.0f };

    // Daylight through the skylights.
    MeshLighting MakeLighting(const WorldGrid& world)
    {
        MeshLighting lighting;
        lighting.directional.direction = Normalize({ 0.6f, -0.9f, 0.45f });
        lighting.directional.color     = { 1.0f, 0.97f, 0.9f, 1.0f };
        lighting.directional.intensity = 0.8f;
        lighting.light.position        = world.TileCenter(GetWarehouseCenter()) + Vec3{ 0.0f, 6.0f, 0.0f };
        lighting.light.intensity       = 0.3f;
        lighting.light.range           = 30.0f;
        lighting.ambient               = { 0.30f, 0.30f, 0.33f, 1.0f };
        lighting.specular              = 0.1f;
        return lighting;
    }

    constexpr TileCoord kCrateTiles[] = {
        { WorldLevel::Ground, 14, 12 }, { WorldLevel::Ground, 15, 12 }, { WorldLevel::Ground, 14, 13 },
        { WorldLevel::Ground, 25, 20 }, { WorldLevel::Ground, 25, 21 }, { WorldLevel::Ground, 24, 21 },
    };
}

WarehouseScene::WarehouseScene(const SceneServices& services)
    : m_state(services.state)
    , m_view(services, WorldLevel::Ground, GetWarehouseCenter(), MakeLighting(services.state.world))
    , m_crate(LoadOrFatal(services.meshes.Load(kCrateMesh), kCrateMesh))
{
}

WarehouseScene::~WarehouseScene()
{
#if defined(GAME_IMGUI_ENABLED)
    if (m_debugPad)
        m_debugPad->UnregisterCategory("Warehouse camera");
#endif
}

void WarehouseScene::Update(const SceneFrame& frame)
{
    m_view.Update(frame);
}

void WarehouseScene::Draw(const SceneRenderers& renderers) const
{
    m_view.Draw(renderers, [this](MeshRenderer::Scope& meshes) {
        for (const TileCoord& tile : kCrateTiles)
            meshes.Draw(*m_crate, WorldView::TileTransform(m_state.world, tile), kCrateColor);
    });
}

#if defined(GAME_IMGUI_ENABLED)
void WarehouseScene::RegisterCheats(DebugPad& pad)
{
    m_debugPad = &pad;
    pad.RegisterCategory("Warehouse camera", [this]() { m_view.GetCamera().DrawCheats(); });
}
#endif
