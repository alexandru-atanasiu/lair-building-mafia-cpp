#include "level_view.h"

#include "../character/character_view.h"
#include "../ui/ui.h"
#include "../world/world_view.h"
#include "platform/window.h"

LevelView::LevelView(const SceneServices& services, WorldLevel level, const TileCoord& startFocus, const MeshLighting& lighting)
    : m_services(services)
    , m_level(level)
    , m_camera(services.window)
    , m_lighting(lighting)
{
    const WorldGrid& world = services.state.world;
    const float      halfX = 0.5f * kTileSize * (float)world.GetWidth();
    const float      halfZ = 0.5f * kTileSize * (float)world.GetDepth();
    m_camera.SetBounds({ -halfX, -halfZ }, { halfX, halfZ });
    m_camera.SetGroundHeight(LevelFloorY(level));
    m_camera.Reset(world.TileCenter(startFocus));
}

namespace
{
    // On the stairs, someone shows in whichever view he's in sight of: the warehouse sees the top
    // of the flight, the lair the bottom, up to where its stairs are cut off (`stairs_cut`).
    constexpr float kGroundShowsAboveY = -2.2f;
    constexpr float kLairShowsBelowY   = -2.7f;
}

bool LevelView::IsShownHere(const CrewMember& member) const
{
    if (member.away)
        return false;
    return m_level == WorldLevel::Ground ? member.motion.position.y > kGroundShowsAboveY : member.motion.position.y < kLairShowsBelowY;
}

void LevelView::Update(const SceneFrame& frame)
{
    m_camera.Update(frame.realSeconds, frame.keyboardAvailable, frame.mouseAvailable && !frame.ui.WantsMouse());

    for (const CrewMember& member : m_services.state.crew)
        if (IsShownHere(member))
            m_services.characters.Animate(member.id, member.motion, frame.sim.simSeconds);
}

void LevelView::DrawLevel(MeshRenderer::Scope& meshes) const
{
    m_services.worldView.Draw(meshes, m_services.state.world, m_level);
}

void LevelView::DrawCrew(const SceneRenderers& renderers) const
{
    SkinnedMeshRenderer::Scope skinned = renderers.skinned.Begin(m_camera.GetCamera(), m_lighting);
    for (const CrewMember& member : m_services.state.crew)
        if (IsShownHere(member))
            m_services.characters.Draw(skinned, member.id, member.motion, member.tint);
}

bool LevelView::TileUnderCursor(TileCoord& tile) const
{
    const Vec2 cursor = { (float)m_services.window.GetMouseX(), (float)m_services.window.GetMouseY() };
    Vec3       ground;
    if (!m_camera.ScreenToGround(cursor, ground))
        return false;
    tile = m_services.state.world.WorldToTile(m_level, ground);
    return m_services.state.world.IsWalkable(tile);
}
