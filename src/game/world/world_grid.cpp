#include "world_grid.h"

#include "core/assert.h"

#include <cmath>
#include <cstdlib>

namespace
{
    constexpr Tile kOutOfBounds = {};
}

WorldGrid::WorldGrid(int width, int depth)
    : m_width(width)
    , m_depth(depth)
{
    const size_t levels = (size_t)kWorldLevelCount;
    m_tiles.resize(levels * (size_t)width * (size_t)depth);
    m_edgesX.resize(levels * (size_t)depth * (size_t)(width + 1), EdgeKind::None);
    m_edgesZ.resize(levels * (size_t)(depth + 1) * (size_t)width, EdgeKind::None);
}

size_t WorldGrid::TileIndex(const TileCoord& c) const
{
    return ((size_t)c.level * (size_t)m_depth + (size_t)c.z) * (size_t)m_width + (size_t)c.x;
}

const Tile& WorldGrid::GetTile(const TileCoord& c) const
{
    return InBounds(c) ? m_tiles[TileIndex(c)] : kOutOfBounds;
}

void WorldGrid::SetTileKind(const TileCoord& c, TileKind kind)
{
    if (!InBounds(c))
        return;
    m_tiles[TileIndex(c)].kind = kind;
    ++m_revision;
}

void WorldGrid::SetTileRoom(const TileCoord& c, uint16_t room)
{
    if (InBounds(c))
        m_tiles[TileIndex(c)].room = room;
}

void WorldGrid::SetTileSolid(const TileCoord& c, bool solid)
{
    if (!InBounds(c))
        return;
    m_tiles[TileIndex(c)].solid = solid;
    ++m_revision;
}

EdgeKind* WorldGrid::FindEdge(const TileCoord& c, EdgeSide side)
{
    if (!InBounds(c))
        return nullptr;
    const size_t level = (size_t)c.level;
    switch (side)
    {
    case EdgeSide::PosX: return &m_edgesX[(level * (size_t)m_depth + (size_t)c.z) * (size_t)(m_width + 1) + (size_t)(c.x + 1)];
    case EdgeSide::NegX: return &m_edgesX[(level * (size_t)m_depth + (size_t)c.z) * (size_t)(m_width + 1) + (size_t)c.x];
    case EdgeSide::PosZ: return &m_edgesZ[(level * (size_t)(m_depth + 1) + (size_t)(c.z + 1)) * (size_t)m_width + (size_t)c.x];
    case EdgeSide::NegZ: return &m_edgesZ[(level * (size_t)(m_depth + 1) + (size_t)c.z) * (size_t)m_width + (size_t)c.x];
    }
    return nullptr;
}

EdgeKind WorldGrid::GetEdge(const TileCoord& c, EdgeSide side) const
{
    const EdgeKind* edge = const_cast<WorldGrid*>(this)->FindEdge(c, side);
    return edge ? *edge : EdgeKind::None;
}

void WorldGrid::SetEdge(const TileCoord& c, EdgeSide side, EdgeKind kind)
{
    if (EdgeKind* edge = FindEdge(c, side))
    {
        *edge = kind;
        ++m_revision;
    }
}

void WorldGrid::AddStair(TileCoord upper, TileCoord lower)
{
    const int dx = lower.x - upper.x;
    const int dz = lower.z - upper.z;
    GAME_ASSERT(upper.level == WorldLevel::Ground && lower.level == WorldLevel::Underground, "Stairs go from ground to underground");
    GAME_ASSERT((std::abs(dx) == kStairSpanTiles && dz == 0) || (dx == 0 && std::abs(dz) == kStairSpanTiles),
                "Stairs must span %d tiles in a straight line", kStairSpanTiles);

    const int stepX = (dx > 0) - (dx < 0);
    const int stepZ = (dz > 0) - (dz < 0);
    for (int i = 1; i < kStairSpanTiles; ++i)
    {
        SetTileKind({ WorldLevel::Ground, upper.x + stepX * i, upper.z + stepZ * i }, TileKind::Stairwell);
        SetTileKind({ WorldLevel::Underground, upper.x + stepX * i, upper.z + stepZ * i }, TileKind::Stairwell);
    }
    m_stairs.push_back({ upper, lower });
    ++m_revision;
}

bool WorldGrid::IsWalkable(const TileCoord& c) const
{
    if (!InBounds(c))
        return false;
    const Tile& tile = GetTile(c);
    return !tile.solid && (tile.kind == TileKind::Floor || tile.kind == TileKind::Pavement);
}

bool WorldGrid::CanStep(const TileCoord& from, int dx, int dz) const
{
    const TileCoord to = { from.level, from.x + dx, from.z + dz };
    if (!IsWalkable(to))
        return false;

    const EdgeSide sideX = dx > 0 ? EdgeSide::PosX : EdgeSide::NegX;
    const EdgeSide sideZ = dz > 0 ? EdgeSide::PosZ : EdgeSide::NegZ;
    if (dz == 0)
        return GetEdge(from, sideX) != EdgeKind::Wall;
    if (dx == 0)
        return GetEdge(from, sideZ) != EdgeKind::Wall;

    const TileCoord cornerX = { from.level, from.x + dx, from.z };
    const TileCoord cornerZ = { from.level, from.x, from.z + dz };
    return IsWalkable(cornerX) && IsWalkable(cornerZ) &&
           GetEdge(from, sideX) == EdgeKind::None && GetEdge(from, sideZ) == EdgeKind::None &&
           GetEdge(cornerX, sideZ) == EdgeKind::None && GetEdge(cornerZ, sideX) == EdgeKind::None;
}

Vec3 WorldGrid::TileCenter(const TileCoord& c) const
{
    return { ((float)c.x + 0.5f - 0.5f * (float)m_width) * kTileSize, LevelFloorY(c.level),
             ((float)c.z + 0.5f - 0.5f * (float)m_depth) * kTileSize };
}

TileCoord WorldGrid::WorldToTile(WorldLevel level, Vec3 position) const
{
    return { level, (int)std::floor(position.x / kTileSize + 0.5f * (float)m_width),
             (int)std::floor(position.z / kTileSize + 0.5f * (float)m_depth) };
}
