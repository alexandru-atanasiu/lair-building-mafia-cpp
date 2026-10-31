#include "lair_view.h"

#include "core/fatal.h"
#include "render/camera_3d.h"
#include "render/mesh.h"
#include "render/mesh_cache.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr WorldLevel kLevel = WorldLevel::Underground;
    constexpr float      kFloorY = kUndergroundFloorY;

    constexpr const char* kFloorMesh     = "meshes/world/floor_tile.mesh";
    constexpr const char* kEarthMeshes[3] = { "meshes/world/earth_rock_a.mesh", "meshes/world/earth_rock_b.mesh", "meshes/world/earth_rock_c.mesh" };
    constexpr const char* kWallMesh      = "meshes/world/wall_segment.mesh";
    constexpr const char* kDoorFrameMesh = "meshes/world/lair_door_frame.mesh";
    constexpr const char* kDoorLeafMesh  = "meshes/world/door_leaf.mesh";
    constexpr const char* kStairsMesh    = "meshes/world/stairs_cut.mesh";
    constexpr const char* kChipMesh      = "meshes/world/rock_chip.mesh";

    // Cutaway.
    constexpr float kLowEarth     = 0.16f; // earth in front of a dug tile sinks to this
    constexpr float kLowWall      = 0.42f; // walls in front of a floor: a curb that still shows the room
    constexpr float kEaseRate     = 9.0f;  // per second, toward the target height
    constexpr float kSampleStep   = 0.4f;  // metres along the line of sight
    constexpr float kEarthAcross  = 0.9f;  // sight lines side by side across a block of earth
    constexpr float kWallAcross   = 0.75f; // and along a wall

    // Doors.
    constexpr float kDoorGap      = 1.2f;  // py/create_meshes.py
    constexpr float kDoorOpenDist = 1.9f;  // someone this near opens it
    constexpr float kDoorOpen     = 1.6f;  // radians
    constexpr float kDoorSpeed    = 4.5f;  // radians per sim second

    const Color kFloorColors[2]  = { { 0.40f, 0.37f, 0.33f, 1.0f }, { 0.37f, 0.34f, 0.30f, 1.0f } };
    const Color kWallColor       = { 0.62f, 0.56f, 0.48f, 1.0f };
    const Color kFrameColor      = { 0.36f, 0.24f, 0.14f, 1.0f };
    const Color kLeafColor       = { 0.52f, 0.34f, 0.18f, 1.0f };
    const Color kStairsColor     = { 0.50f, 0.47f, 0.42f, 1.0f };
    const Color kRockHigh        = { 0.24f, 0.20f, 0.17f, 1.0f }; // the top of the rock
    const Color kRockCut         = { 0.33f, 0.27f, 0.22f, 1.0f }; // where it's cut away: paler, like a section
    const Color kChipColor       = { 0.46f, 0.36f, 0.26f, 1.0f };

    const Color kDigMark         = { 0.95f, 0.72f, 0.15f, 0.40f };
    const Color kDigWaiting      = { 0.70f, 0.62f, 0.42f, 0.22f }; // marked, but nobody can get to it yet
    const Color kSiteOverlay     = { 0.35f, 0.65f, 0.95f, 0.22f };
    const Color kWallBlueprint   = { 0.45f, 0.75f, 1.00f, 0.30f };
    const Color kDoorBlueprint   = { 0.45f, 0.75f, 1.00f, 0.55f };

    Color Lerp(const Color& a, const Color& b, float t)
    {
        return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t };
    }

    float Ease(float value, float target, float seconds)
    {
        return value + (target - value) * std::min(1.0f, kEaseRate * seconds);
    }

    // A steady scatter by tile, so the rock isn't a checkerboard.
    uint32_t TileHash(int x, int z)
    {
        uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)z * 19349663u;
        h ^= h >> 13;
        h *= 0x5bd1e995u;
        return h ^ (h >> 15);
    }

    const Mesh* LoadMesh(MeshCache& cache, const char* path, std::shared_ptr<const Mesh>& slot)
    {
        slot = LoadOrFatal(cache.Load(path), path);
        return slot.get();
    }
}

LairView::LairView(const SceneServices& services)
    : m_state(services.state)
    , m_width(services.state.world.GetWidth())
    , m_depth(services.state.world.GetDepth())
{
    MeshCache& cache = services.meshes;
    LoadMesh(cache, kFloorMesh, m_floor);
    for (int i = 0; i < 3; ++i)
        LoadMesh(cache, kEarthMeshes[i], m_earth[i]);
    LoadMesh(cache, kWallMesh, m_wall);
    LoadMesh(cache, kDoorFrameMesh, m_doorFrame);
    LoadMesh(cache, kDoorLeafMesh, m_doorLeaf);
    LoadMesh(cache, kStairsMesh, m_stairs);
    LoadMesh(cache, kChipMesh, m_chip);

    for (const LairObjectType& type : GetLairObjectTypes())
    {
        ObjectParts loaded;
        for (const LairObjectPart& part : type.parts)
            if (part.mesh)
                loaded.parts.push_back({ LoadOrFatal(cache.Load(part.mesh), part.mesh), part.color });
        m_objectParts.push_back(std::move(loaded));
    }

    const size_t tiles = (size_t)m_width * (size_t)m_depth;
    m_earthHeight.assign(tiles, kWallHeight);
    m_earthTarget.assign(tiles, kWallHeight);
    m_edgeXHeight.assign((size_t)(m_width + 1) * (size_t)m_depth, kWallHeight);
    m_edgeXTarget.assign((size_t)(m_width + 1) * (size_t)m_depth, kWallHeight);
    m_edgeZHeight.assign((size_t)m_width * (size_t)(m_depth + 1), kWallHeight);
    m_edgeZTarget.assign((size_t)m_width * (size_t)(m_depth + 1), kWallHeight);
}

bool LairView::IsOpen(const TileCoord& tile) const
{
    if (!m_state.world.InBounds(tile))
        return false;
    const TileKind kind = m_state.world.GetTile(tile).kind;
    return kind == TileKind::Floor || kind == TileKind::Stairwell;
}

float LairView::AllowedHeight(Vec3 from, Vec3 across, const TileCoord* self) const
{
    const WorldGrid& world = m_state.world;
    float allowed = kWallHeight;

    for (const float side : { 0.0f, 1.0f, -1.0f })
    {
        const Vec3  start = from + across * side;
        const float dx = start.x - m_cameraPosition.x;
        const float dz = start.z - m_cameraPosition.z;
        const float flat = std::sqrt(dx * dx + dz * dz);
        if (flat < 0.01f)
            continue; // straight below the camera: nothing hides anything

        const float rise = std::max(m_cameraPosition.y - kFloorY, 0.1f) / flat; // sight line's slope
        const float reach = allowed / rise;
        const Vec3  dir = { dx / flat, 0.0f, dz / flat };

        float exit = 0.0f; // where the line leaves this block
        for (float s = 0.03f; s < reach + kTileSize * 1.5f; s += kSampleStep)
        {
            const TileCoord tile = world.WorldToTile(kLevel, start + dir * s);
            if (self && tile == *self)
            {
                exit = s;
                continue;
            }
            const float gap = std::max(s - exit - kSampleStep, 0.0f);
            if (gap * rise >= allowed)
                break;
            if (IsOpen(tile))
            {
                allowed = gap * rise;
                break;
            }
        }
    }
    return allowed;
}

void LairView::ComputeTargets()
{
    const WorldGrid& world = m_state.world;
    for (int z = 0; z < m_depth; ++z)
    {
        for (int x = 0; x < m_width; ++x)
        {
            const TileCoord tile = { kLevel, x, z };
            const Vec3      center = world.TileCenter(tile);
            if (world.GetTile(tile).kind == TileKind::Earth)
            {
                // Only look across the line of sight: toward the camera doesn't matter.
                const float dx = center.x - m_cameraPosition.x;
                const float dz = center.z - m_cameraPosition.z;
                const float flat = std::max(std::sqrt(dx * dx + dz * dz), 0.01f);
                const Vec3  across = Vec3{ -dz / flat, 0.0f, dx / flat } * kEarthAcross;
                m_earthTarget[TileIndex(x, z)] = std::max(AllowedHeight(center, across, &tile), kLowEarth);
            }
        }
    }

    // Any edge that shows something; the others cost nothing to compute lazily, but pending rooms
    // ask about edges that aren't walls yet, so all edges next to floor get a target.
    for (int z = 0; z < m_depth; ++z)
    {
        for (int x = -1; x < m_width; ++x)
        {
            if (z < 0)
                continue;
            const TileCoord a = { kLevel, x, z };
            const TileCoord b = { kLevel, x + 1, z };
            if (!IsOpen(a) && !IsOpen(b))
                continue;
            const LairEdge edge = { a, EdgeSide::PosX };
            m_edgeXTarget[EdgeXIndex(x, z)] = std::max(AllowedHeight(GetLairEdgeCenter(m_state, edge), Vec3{ 0.0f, 0.0f, kWallAcross }, nullptr), kLowWall);
        }
    }
    for (int z = -1; z < m_depth; ++z)
    {
        for (int x = 0; x < m_width; ++x)
        {
            const TileCoord a = { kLevel, x, z };
            const TileCoord b = { kLevel, x, z + 1 };
            if (!IsOpen(a) && !IsOpen(b))
                continue;
            const LairEdge edge = { a, EdgeSide::PosZ };
            m_edgeZTarget[EdgeZIndex(x, z)] = std::max(AllowedHeight(GetLairEdgeCenter(m_state, edge), Vec3{ kWallAcross, 0.0f, 0.0f }, nullptr), kLowWall);
        }
    }
}

void LairView::Update(const Camera3D& camera, float realSeconds, float simSeconds)
{
    const Vec3 position = camera.GetPosition();
    const bool moved = Length(position - m_cameraPosition) > 0.01f;
    if (moved || m_targetsRevision != m_state.world.GetRevision())
    {
        m_cameraPosition  = position;
        m_targetsRevision = m_state.world.GetRevision();
        ComputeTargets();
    }

    for (size_t i = 0; i < m_earthHeight.size(); ++i)
        m_earthHeight[i] = Ease(m_earthHeight[i], m_earthTarget[i], realSeconds);
    for (size_t i = 0; i < m_edgeXHeight.size(); ++i)
        m_edgeXHeight[i] = Ease(m_edgeXHeight[i], m_edgeXTarget[i], realSeconds);
    for (size_t i = 0; i < m_edgeZHeight.size(); ++i)
        m_edgeZHeight[i] = Ease(m_edgeZHeight[i], m_edgeZTarget[i], realSeconds);

    m_time += simSeconds;

    // Doors swing away from whoever comes near, and fall shut behind them.
    for (const LairDoor& door : m_state.lair.doors)
    {
        float& angle = m_doorAngles[door.id];
        if (!door.built)
        {
            angle = 0.0f;
            continue;
        }
        const Vec3 center = GetLairEdgeCenter(m_state, door.edge);
        float nearest = kDoorOpenDist;
        float wanted  = 0.0f;
        const auto someoneAt = [&](const CharacterMotion& motion) {
            if (motion.level != kLevel)
                return;
            const Vec3  d = motion.position - center;
            const float dist = std::sqrt(d.x * d.x + d.z * d.z);
            if (dist >= nearest)
                return;
            nearest = dist;
            const float along = door.edge.side == EdgeSide::PosX ? d.x : d.z; // door-local z
            // An open door stays open the way it went, even once they've passed through.
            wanted = angle != 0.0f ? (angle > 0.0f ? kDoorOpen : -kDoorOpen) : (along < 0.0f ? -kDoorOpen : kDoorOpen);
        };
        for (const CrewMember& member : m_state.crew)
            if (!member.away)
                someoneAt(member.motion);
        for (const LawAgent& agent : m_state.law.agents)
            someoneAt(agent.motion);
        const float step = kDoorSpeed * simSeconds;
        angle += std::clamp(wanted - angle, -step, step);
    }
}

float LairView::GetSurfaceHeight(const TileCoord& tile) const
{
    if (tile.level != kLevel || !m_state.world.InBounds(tile) || m_state.world.GetTile(tile).kind != TileKind::Earth)
        return 0.0f;
    float height = m_earthHeight[TileIndex(tile.x, tile.z)];
    if (const LairJob* job = FindLairDigJob(m_state, tile))
        height = std::max(height * (1.0f - job->progress / job->duration), 0.1f);
    return height;
}

float LairView::GetEdgeHeight(const LairEdge& edge) const
{
    if (edge.tile.z < 0 || edge.tile.z >= m_depth + 1 || edge.tile.x < -1 || edge.tile.x >= m_width)
        return 0.0f;
    return edge.side == EdgeSide::PosX ? m_edgeXHeight[EdgeXIndex(edge.tile.x, edge.tile.z)] : m_edgeZHeight[EdgeZIndex(edge.tile.x, edge.tile.z)];
}

bool LairView::PickTile(Vec3 origin, Vec3 direction, TileCoord& tile) const
{
    if (direction.y >= -1e-5f)
        return false;
    const WorldGrid& world = m_state.world;
    // Down the ray from the top of the rock: the first block as tall as the ray is high.
    constexpr float kStep = 0.08f;
    for (float y = kWallHeight + 0.1f; y > 0.0f; y -= kStep)
    {
        const Vec3      p = origin + direction * ((kFloorY + y - origin.y) / direction.y);
        const TileCoord c = world.WorldToTile(kLevel, p);
        if (world.InBounds(c) && world.GetTile(c).kind == TileKind::Earth && GetSurfaceHeight(c) >= y)
        {
            tile = c;
            return true;
        }
    }
    tile = world.WorldToTile(kLevel, origin + direction * ((kFloorY - origin.y) / direction.y));
    return world.InBounds(tile);
}

Mat4 LairView::EdgeTransform(const LairEdge& edge) const
{
    return Mat4::Translation(GetLairEdgeCenter(m_state, edge)) * Mat4::RotationY(edge.side == EdgeSide::PosX ? 0.5f * kPi : 0.0f);
}

void LairView::DrawEarth(MeshRenderer::Scope& meshes, const TileCoord& tile) const
{
    const uint32_t hash   = TileHash(tile.x, tile.z);
    const float    height = GetSurfaceHeight(tile);
    const float    t      = std::clamp((height - kLowEarth) / (kWallHeight - kLowEarth), 0.0f, 1.0f);
    const float    shade  = 0.88f + 0.24f * (float)(hash % 256) / 255.0f;
    Color          color  = Lerp(kRockCut, kRockHigh, t);
    color.r *= shade;
    color.g *= shade;
    color.b *= shade;

    const Mat4 at = Mat4::Translation(m_state.world.TileCenter(tile)) * Mat4::RotationY((float)((hash >> 8) % 4) * 0.5f * kPi) *
                    Mat4::Scale({ 1.0f, height / kWallHeight, 1.0f });
    meshes.Draw(*m_earth[(hash >> 16) % 3], at, color);
}

void LairView::DrawWall(MeshRenderer::Scope& meshes, const LairEdge& edge, float height, const Color& color) const
{
    meshes.Draw(*m_wall, EdgeTransform(edge) * Mat4::Scale({ 1.0f, std::max(height, 0.02f) / kWallHeight, 1.0f }), color);
}

void LairView::Draw(MeshRenderer::Scope& meshes) const
{
    const WorldGrid& world = m_state.world;

    for (int z = 0; z < m_depth; ++z)
    {
        for (int x = 0; x < m_width; ++x)
        {
            const TileCoord tile = { kLevel, x, z };
            const Tile&     data = world.GetTile(tile);
            if (data.kind == TileKind::Earth)
            {
                DrawEarth(meshes, tile);
                // Digging throws up bits of rock.
                if (const LairJob* job = FindLairDigJob(m_state, tile); job && job->progress > 0.0f)
                {
                    const Vec3 center = world.TileCenter(tile);
                    for (int i = 0; i < 4; ++i)
                    {
                        const float phase = std::fmod(m_time * 2.3f + (float)i * 0.27f, 1.0f);
                        const float angle = (float)i * 1.7f + (float)(x + z);
                        const Vec3  offset = { std::cos(angle) * (0.3f + phase * 0.6f), 0.2f + GetSurfaceHeight(tile) + std::sin(phase * kPi) * 0.7f, std::sin(angle) * (0.3f + phase * 0.6f) };
                        meshes.Draw(*m_chip, Mat4::Translation(center + offset), kChipColor);
                    }
                }
            }
            else if (data.kind == TileKind::Floor)
            {
                Color color = kFloorColors[(x + z) & 1];
                if (data.room != 0)
                {
                    if (const LairRoom* room = FindLairRoom(m_state, data.room))
                    {
                        const Color& tint = GetLairRoomTypes()[(size_t)room->type].floorTint;
                        const float  amount = room->built ? 0.85f : 0.45f;
                        const float  shade = (x + z) & 1 ? 0.94f : 1.0f;
                        color = Lerp(color, { tint.r * shade, tint.g * shade, tint.b * shade, 1.0f }, amount);
                    }
                }
                meshes.Draw(*m_floor, Mat4::Translation(world.TileCenter(tile)), color);
            }
        }
    }

    // Walls and doors, each edge once.
    const auto drawEdge = [&](const LairEdge& edge) {
        const EdgeKind kind = world.GetEdge(edge.tile, edge.side);
        if (kind == EdgeKind::None)
            return;
        const float height = GetEdgeHeight(edge);
        if (kind == EdgeKind::Wall)
        {
            DrawWall(meshes, edge, height, kWallColor);
            return;
        }
        // A door: the frame sinks with the wall; the leaf too, but keeps enough height to be seen swinging.
        const float scale     = height / kWallHeight;
        const float leafScale = std::max(scale, 0.3f);
        meshes.Draw(*m_doorFrame, EdgeTransform(edge) * Mat4::Scale({ 1.0f, scale, 1.0f }), kFrameColor);
        float angle = 0.0f;
        if (const LairDoor* door = FindLairDoor(m_state, edge))
        {
            const auto found = m_doorAngles.find(door->id);
            if (found != m_doorAngles.end())
                angle = found->second;
        }
        meshes.Draw(*m_doorLeaf,
                    EdgeTransform(edge) * Mat4::Translation({ -0.5f * kDoorGap, 0.0f, 0.0f }) * Mat4::RotationY(angle) * Mat4::Scale({ 1.0f, leafScale, 1.0f }),
                    kLeafColor);
    };
    for (int z = 0; z < m_depth; ++z)
    {
        for (int x = 0; x < m_width; ++x)
        {
            drawEdge({ { kLevel, x, z }, EdgeSide::PosX });
            drawEdge({ { kLevel, x, z }, EdgeSide::PosZ });
        }
    }

    // Walls on their way up.
    for (const LairRoom& room : m_state.lair.rooms)
    {
        if (room.built)
            continue;
        const LairJob* job = FindLairJobFor(m_state, LairJobKind::Room, room.id);
        const float    progress = job ? std::min(job->progress / job->duration, 1.0f) : 0.0f;
        if (progress <= 0.0f)
            continue;
        const auto rising = [&](const LairEdge& edge) {
            if (world.GetEdge(edge.tile, edge.side) == EdgeKind::None)
                DrawWall(meshes, edge, GetEdgeHeight(edge) * progress, kWallColor);
        };
        for (int x = room.rect.x0; x <= room.rect.x1; ++x)
        {
            rising(MakeLairEdge({ kLevel, x, room.rect.z0 }, EdgeSide::NegZ));
            rising(MakeLairEdge({ kLevel, x, room.rect.z1 }, EdgeSide::PosZ));
        }
        for (int z = room.rect.z0; z <= room.rect.z1; ++z)
        {
            rising(MakeLairEdge({ kLevel, room.rect.x0, z }, EdgeSide::NegX));
            rising(MakeLairEdge({ kLevel, room.rect.x1, z }, EdgeSide::PosX));
        }
    }

    // Objects.
    for (const LairObject& object : m_state.lair.objects)
    {
        if (!object.built)
            continue;
        const LairObjectType& type = GetLairObjectTypes()[(size_t)object.type];
        const Mat4 at = Mat4::Translation(GetLairObjectCenter(m_state, type, object.tile, object.rotation)) * Mat4::RotationY((float)object.rotation * 0.5f * kPi);
        for (const auto& part : m_objectParts[(size_t)object.type].parts)
            meshes.Draw(*part.first, at, part.second);
    }

    // The mesh goes down along +Z from the upper tile.
    for (const Stair& stair : world.GetStairs())
    {
        const float yaw = std::atan2((float)(stair.lower.x - stair.upper.x), (float)(stair.lower.z - stair.upper.z));
        meshes.Draw(*m_stairs, Mat4::Translation(world.TileCenter(stair.upper)) * Mat4::RotationY(yaw), kStairsColor);
    }
}

void LairView::DrawTileOverlay(MeshRenderer::Scope& scope, const TileCoord& tile, const Color& color) const
{
    const Vec3 center = m_state.world.TileCenter(tile) + Vec3{ 0.0f, GetSurfaceHeight(tile) + 0.04f, 0.0f };
    scope.Draw(*m_floor, Mat4::Translation(center), color, 1.0f);
}

void LairView::DrawWallGhost(MeshRenderer::Scope& scope, const LairEdge& edge, float height, const Color& color) const
{
    scope.Draw(*m_wall, EdgeTransform(edge) * Mat4::Scale({ 1.02f, std::max(height, 0.05f) / kWallHeight, 1.1f }), color, 1.0f);
}

void LairView::DrawDoorGhost(MeshRenderer::Scope& scope, const LairEdge& edge, const Color& color) const
{
    const float height = std::max(GetEdgeHeight(edge), 0.8f) + 0.05f;
    scope.Draw(*m_doorFrame, EdgeTransform(edge) * Mat4::Scale({ 1.02f, height / kWallHeight, 1.3f }), color, 1.0f);
    scope.Draw(*m_doorLeaf, EdgeTransform(edge) * Mat4::Translation({ -0.5f * kDoorGap, 0.0f, 0.0f }) * Mat4::Scale({ 1.0f, height / kWallHeight, 1.5f }), color, 1.0f);
}

void LairView::DrawObjectGhost(MeshRenderer::Scope& scope, int type, const TileCoord& tile, int rotation, const Color& tint, bool tinted, float grow) const
{
    const LairObjectType& def = GetLairObjectTypes()[(size_t)type];
    const Mat4 at = Mat4::Translation(GetLairObjectCenter(m_state, def, tile, rotation)) * Mat4::RotationY((float)rotation * 0.5f * kPi) *
                    Mat4::Scale({ 1.0f, std::max(grow, 0.05f), 1.0f });
    for (const auto& part : m_objectParts[(size_t)type].parts)
    {
        Color color = tinted ? tint : part.second;
        color.a = tint.a;
        scope.Draw(*part.first, at, color, tinted ? 0.75f : 0.2f);
    }
}

void LairView::DrawOverlays(MeshRenderer::Scope& translucent) const
{
    for (const LairJob& job : m_state.lair.jobs)
    {
        if (job.kind == LairJobKind::Dig)
        {
            DrawTileOverlay(translucent, job.tile, IsLairDigReachable(m_state, job.tile) ? kDigMark : kDigWaiting);
        }
        else if (job.kind == LairJobKind::Door)
        {
            for (const LairDoor& door : m_state.lair.doors)
                if (door.id == job.target)
                    DrawDoorGhost(translucent, door.edge, kDoorBlueprint);
        }
        else if (job.kind == LairJobKind::Object)
        {
            for (const LairObject& object : m_state.lair.objects)
            {
                if (object.id != job.target)
                    continue;
                const float progress = std::min(job.progress / job.duration, 1.0f);
                DrawObjectGhost(translucent, object.type, object.tile, object.rotation, { 1.0f, 1.0f, 1.0f, 0.45f + 0.45f * progress }, false);
                for (const TileCoord& tile : GetLairObjectTiles(GetLairObjectTypes()[(size_t)object.type], object.tile, object.rotation))
                    DrawTileOverlay(translucent, tile, kSiteOverlay);
            }
        }
    }

    for (const LairRoom& room : m_state.lair.rooms)
    {
        if (room.built)
            continue;
        for (int z = room.rect.z0; z <= room.rect.z1; ++z)
            for (int x = room.rect.x0; x <= room.rect.x1; ++x)
                DrawTileOverlay(translucent, { kLevel, x, z }, kSiteOverlay);

        const auto blueprint = [&](const LairEdge& edge) {
            if (m_state.world.GetEdge(edge.tile, edge.side) == EdgeKind::None)
                DrawWallGhost(translucent, edge, GetEdgeHeight(edge), kWallBlueprint);
        };
        for (int x = room.rect.x0; x <= room.rect.x1; ++x)
        {
            blueprint(MakeLairEdge({ kLevel, x, room.rect.z0 }, EdgeSide::NegZ));
            blueprint(MakeLairEdge({ kLevel, x, room.rect.z1 }, EdgeSide::PosZ));
        }
        for (int z = room.rect.z0; z <= room.rect.z1; ++z)
        {
            blueprint(MakeLairEdge({ kLevel, room.rect.x0, z }, EdgeSide::NegX));
            blueprint(MakeLairEdge({ kLevel, room.rect.x1, z }, EdgeSide::PosX));
        }
    }
}
