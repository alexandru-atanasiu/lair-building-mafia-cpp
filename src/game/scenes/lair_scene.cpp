#include "lair_scene.h"

#include "../ui/ui.h"
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

    // A dim cellar: one warm lamp over the basement, a soft fill from above to keep the far
    // corners readable.
    MeshLighting MakeLighting(const WorldGrid& world)
    {
        MeshLighting lighting;
        lighting.directional.direction = Normalize({ 0.4f, -1.0f, 0.3f });
        lighting.directional.color     = { 0.8f, 0.82f, 0.9f, 1.0f };
        lighting.directional.intensity = 0.45f;
        lighting.light.position        = world.TileCenter(GetBasementCenter()) + Vec3{ 0.0f, 5.0f, 0.0f };
        lighting.light.color           = { 1.0f, 0.85f, 0.6f, 1.0f };
        lighting.light.intensity       = 0.9f;
        lighting.light.range           = 34.0f;
        lighting.ambient               = { 0.24f, 0.23f, 0.25f, 1.0f };
        lighting.specular              = 0.08f;
        return lighting;
    }

    constexpr TileCoord kCrateTiles[] = {
        { WorldLevel::Underground, 16, 18 }, { WorldLevel::Underground, 17, 18 }, { WorldLevel::Underground, 16, 19 },
        { WorldLevel::Underground, 24, 27 }, { WorldLevel::Underground, 25, 27 },
    };
}

LairScene::LairScene(const SceneServices& services)
    : m_state(services.state)
    , m_view(services, WorldLevel::Underground, GetBasementCenter(), MakeLighting(services.state.world))
    , m_crate(LoadOrFatal(services.meshes.Load(kCrateMesh), kCrateMesh))
{
}

LairScene::~LairScene()
{
#if defined(GAME_IMGUI_ENABLED)
    if (m_debugPad)
        m_debugPad->UnregisterCategory("Lair camera");
#endif
}

void LairScene::Update(const SceneFrame& frame)
{
    m_view.Update(frame);

    TileCoord tile;
    if (m_view.GetCamera().WasRightClicked() && m_view.TileUnderCursor(tile) && !m_state.crew.empty())
        SendCharacterTo(m_state.crew.front().motion, m_state.world, tile);
}

void LairScene::Draw(const SceneRenderers& renderers) const
{
    m_view.Draw(renderers, [this](MeshRenderer::Scope& meshes) {
        for (const TileCoord& tile : kCrateTiles)
            meshes.Draw(*m_crate, WorldView::TileTransform(m_state.world, tile), kCrateColor);
    });
}

#if defined(GAME_IMGUI_ENABLED)
void LairScene::RegisterCheats(DebugPad& pad)
{
    m_debugPad = &pad;
    pad.RegisterCategory("Lair camera", [this]() { m_view.GetCamera().DrawCheats(); });
}
#endif
