#include "lair_scene.h"

#include "../ui/ui.h"
#include "../world/world_layout.h"

#if defined(GAME_IMGUI_ENABLED)
#include "debug/debug_pad.h"
#endif

namespace
{
    // The default view: near enough to read what a man is doing, and turned so that two walls of
    // a room face you. Everything can still be zoomed and turned.
    constexpr float kDefaultFov      = DegToRad(32.0f);
    constexpr float kDefaultDistance = 19.0f;
    constexpr float kDefaultPitch    = DegToRad(-58.0f);
    constexpr float kDefaultYaw      = DegToRad(-28.0f);

    constexpr float kLampHeight = 6.0f; // the lamp that follows the view, above the floor

    // A dim cellar: a warm lamp over what you're looking at, a soft fill from above to keep the
    // far corners readable.
    MeshLighting MakeLighting(const WorldGrid& world)
    {
        MeshLighting lighting;
        lighting.directional.direction = Normalize({ 0.4f, -1.0f, 0.3f });
        lighting.directional.color     = { 0.8f, 0.82f, 0.9f, 1.0f };
        lighting.directional.intensity = 0.5f;
        lighting.light.position        = world.TileCenter(GetBasementCenter()) + Vec3{ 0.0f, kLampHeight, 0.0f };
        lighting.light.color           = { 1.0f, 0.85f, 0.6f, 1.0f };
        lighting.light.intensity       = 0.9f;
        lighting.light.range           = 40.0f;
        lighting.ambient               = { 0.30f, 0.29f, 0.31f, 1.0f };
        lighting.specular              = 0.06f;
        return lighting;
    }
}

LairScene::LairScene(const SceneServices& services)
    : m_view(services, WorldLevel::Underground, GetBasementCenter(), MakeLighting(services.state.world))
    , m_lair(services)
    , m_tools(services, m_lair)
    , m_translucent(std::make_unique<MeshRenderer>(services.device, MeshRenderStyle::Translucent))
{
    m_view.GetCamera().SetVerticalFov(kDefaultFov);
    m_view.GetCamera().SetDefaultView(kDefaultDistance, kDefaultPitch, kDefaultYaw);
    m_view.GetCamera().Reset(services.state.world.TileCenter(GetBasementCenter()));
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
    m_tools.BuildUi(frame.ui, m_view.GetCamera());
    m_view.Update(frame);
    m_view.GetLighting().light.position = m_view.GetCamera().GetFocus() + Vec3{ 0.0f, kLampHeight, 0.0f };
    m_lair.Update(m_view.GetCamera().GetCamera(), frame.realSeconds, frame.sim.simSeconds);
    m_tools.UpdateWorld(frame, m_view.GetCamera());
}

void LairScene::Draw(const SceneRenderers& renderers) const
{
    const Camera3D& camera = m_view.GetCamera().GetCamera();
    {
        MeshRenderer::Scope meshes = renderers.meshes.Begin(camera, m_view.GetLighting());
        m_lair.Draw(meshes);
    }
    m_view.DrawCrew(renderers);
    {
        MeshRenderer::Scope translucent = m_translucent->Begin(camera, m_view.GetLighting());
        m_lair.DrawOverlays(translucent);
        m_tools.DrawGhosts(translucent);
    }
}

#if defined(GAME_IMGUI_ENABLED)
void LairScene::RegisterCheats(DebugPad& pad)
{
    m_debugPad = &pad;
    pad.RegisterCategory("Lair camera", [this]() { m_view.GetCamera().DrawCheats(); });
}
#endif
