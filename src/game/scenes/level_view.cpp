#include "level_view.h"

#include "../character/character_view.h"
#include "../ui/ui.h"
#include "../world/world_view.h"
#include "platform/window.h"

#include <algorithm>

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

    // The tag over someone of the law's, so he can't be missed among the crew, or over a man off work.
    constexpr float kTagHeight = 2.15f; // metres above the feet
    constexpr float kTagText   = 13.0f;
}

bool LevelView::IsShownHere(const CharacterMotion& motion) const
{
    return m_level == WorldLevel::Ground ? motion.position.y > kGroundShowsAboveY : motion.position.y < kLairShowsBelowY;
}

void LevelView::DrawTags(const SceneFrame& frame) const
{
    Ui& ui = frame.ui;
    const float scale = ui.GetScale();
    const auto  tag = [&](const CharacterMotion& motion, const char* text, const Color& color) {
        Vec2 at;
        if (!m_camera.WorldToScreen(motion.position + Vec3{ 0.0f, kTagHeight, 0.0f }, at))
            return;
        Ui::TextStyle style;
        style.size   = kTagText;
        style.bold   = true;
        style.shadow = false;
        style.color  = { 0.05f, 0.05f, 0.05f, 1.0f };
        const float w = ui.MeasureText(text, kTagText, true) + 10.0f;
        const float h = ui.GetLineHeight(kTagText) + 2.0f;
        const float x = at.x / scale - 0.5f * w;
        const float y = at.y / scale - h - 6.0f;
        if (y < ui.GetContentTop())
            return; // not over the HUD
        ui.Fill({ at.x / scale - 1.5f, y + h, 3.0f, 6.0f }, color);
        ui.Fill({ x, y, w, h }, color);
        ui.LabelIn({ x, y, w, h }, text, style);
    };
    for (const LawAgent& agent : m_services.state.law.agents)
    {
        if (!IsShownHere(agent.motion))
            continue;
        if (agent.kind == LawAgentKind::Investigator)
            tag(agent.motion, "Investigator", { 0.95f, 0.78f, 0.30f, 1.0f });
        else
            tag(agent.motion, "Police", { 0.45f, 0.65f, 1.0f, 1.0f });
    }
    if (m_services.state.law.bossRunning)
        for (const CrewMember& member : m_services.state.crew)
            if (member.role == CrewRole::Boss && !member.away && IsShownHere(member.motion))
                tag(member.motion, "To the bolt-hole!", { 1.0f, 0.45f, 0.35f, 1.0f });
    for (const CrewMember& member : m_services.state.crew)
    {
        if (member.jobId != kRestJobId || member.away || !IsShownHere(member.motion))
            continue;
        if (member.inBed)
            tag(member.motion, "Resting", { 0.55f, 0.82f, 0.68f, 1.0f });
        else
            tag(member.motion, "Tired", { 0.85f, 0.65f, 0.55f, 1.0f });
    }
}

void LevelView::Update(const SceneFrame& frame)
{
    m_camera.Update(frame.realSeconds, frame.keyboardAvailable, frame.mouseAvailable && !frame.ui.WantsMouse());

    for (const CrewMember& member : m_services.state.crew)
        if (!member.away && IsShownHere(member.motion))
            m_services.characters.Animate(member.id, member.motion, frame.sim.simSeconds);
    for (const LawAgent& agent : m_services.state.law.agents)
        if (IsShownHere(agent.motion))
            m_services.characters.Animate(agent.id, agent.motion, frame.sim.simSeconds);
    DrawTags(frame);
}

void LevelView::DrawLevel(MeshRenderer::Scope& meshes) const
{
    m_services.worldView.Draw(meshes, m_services.state.world, m_level, m_wallLooks);
}

void LevelView::DrawCrew(const SceneRenderers& renderers) const
{
    SkinnedMeshRenderer::Scope skinned = renderers.skinned.Begin(m_camera.GetCamera(), m_lighting);
    for (const CrewMember& member : m_services.state.crew)
        if (!member.away && IsShownHere(member.motion))
            m_services.characters.Draw(skinned, member.id, member.motion, member.tint);
    for (const LawAgent& agent : m_services.state.law.agents)
        if (IsShownHere(agent.motion))
            m_services.characters.Draw(skinned, agent.id, agent.motion, agent.tint);
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
