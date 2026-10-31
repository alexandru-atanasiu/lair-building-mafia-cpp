#pragma once

#include "core/math.h"

#include <cstdint>
#include <vector>

// The place: two levels of tiles of the same size, ground above underground, with walls and doors
// on tile edges and staircases between the levels. Plain data, nothing here draws (world_view.h
// does) or decides what may be dug or built (that's game rules). Paths: world_path.h.
//
// World space: left-handed, Y up, in metres. Tile (x, z) of a level covers kTileSize squared, and
// the grid is centered on the origin. Edge sides name directions in world space.

constexpr float kTileSize          = 2.0f;
constexpr float kWallHeight        = 2.4f;
constexpr float kGroundFloorY      = 0.0f;
constexpr float kUndergroundFloorY = -4.0f; // py/create_meshes.py's stairs span this drop
constexpr int   kStairSpanTiles    = 3;     // from the upper tile to the lower tile, in a straight line

enum class WorldLevel : uint8_t
{
    Ground,
    Underground,
};
constexpr int kWorldLevelCount = 2;

constexpr float LevelFloorY(WorldLevel level) { return level == WorldLevel::Ground ? kGroundFloorY : kUndergroundFloorY; }

enum class TileKind : uint8_t
{
    Earth,     // solid: not walkable. Digging one out makes it Floor.
    Floor,     // indoors: walkable
    Pavement,  // outdoors on the ground level: walkable
    Stairwell, // the slope of a staircase: only walked through the stair link
    Fixture,   // indoors with something fixed standing on it: drawn as floor, not walkable
};

enum class EdgeKind : uint8_t
{
    None,
    Wall, // blocks walking
    Door, // walkable
};

enum class EdgeSide : uint8_t
{
    PosX,
    NegX,
    PosZ,
    NegZ,
};

struct TileCoord
{
    WorldLevel level = WorldLevel::Ground;
    int        x     = 0;
    int        z     = 0;

    friend bool operator==(const TileCoord&, const TileCoord&) = default;
};

struct Tile
{
    TileKind kind = TileKind::Earth;
    uint16_t room = 0; // 0 = none. Free for the lair rules: an index into their room list.
    bool     solid = false; // something stands here (a bed, a shelf): not walkable
};

// A straight staircase: walked from the upper tile's center to the lower tile's center. The tiles
// between them (kStairSpanTiles - 1 per level) become Stairwell.
struct Stair
{
    TileCoord upper; // on the ground level
    TileCoord lower; // on the underground level
};

class WorldGrid
{
public:
    // Every tile starts as Earth, with no edges.
    WorldGrid(int width, int depth);

    int GetWidth() const { return m_width; }
    int GetDepth() const { return m_depth; }
    bool InBounds(const TileCoord& c) const { return c.x >= 0 && c.z >= 0 && c.x < m_width && c.z < m_depth; }

    // Out of bounds reads as solid Earth.
    const Tile& GetTile(const TileCoord& c) const;
    void        SetTileKind(const TileCoord& c, TileKind kind);
    void        SetTileRoom(const TileCoord& c, uint16_t room);
    void        SetTileSolid(const TileCoord& c, bool solid);

    // The edge on `side` of a tile; the neighbor across it sees the same edge. Edges on the grid's
    // border exist too. Out of bounds reads as None and writes are ignored.
    EdgeKind GetEdge(const TileCoord& c, EdgeSide side) const;
    void     SetEdge(const TileCoord& c, EdgeSide side, EdgeKind kind);

    // Marks the stairwell tiles on both levels. `upper` and `lower` keep their kinds, so make them
    // walkable. They must be kStairSpanTiles apart in a straight line.
    void AddStair(TileCoord upper, TileCoord lower);
    const std::vector<Stair>& GetStairs() const { return m_stairs; }

    // A tile can be stood on.
    bool IsWalkable(const TileCoord& c) const;
    // One step to an 8-neighbor on the same level: both tiles walkable and no wall in the way.
    // A diagonal step also needs its two corner tiles walkable and no edge at all around it, so
    // nobody cuts a corner or squeezes diagonally through a door.
    bool CanStep(const TileCoord& from, int dx, int dz) const;

    // Bumped by every change: movers re-plan, caches rebuild.
    uint64_t GetRevision() const { return m_revision; }

    Vec3 TileCenter(const TileCoord& c) const;
    // The tile under a world position on `level`; it may be out of bounds.
    TileCoord WorldToTile(WorldLevel level, Vec3 position) const;

private:
    size_t TileIndex(const TileCoord& c) const;
    // Edges are stored once: the PosX edges of (x, z) for x in [-1, width), PosZ likewise.
    EdgeKind* FindEdge(const TileCoord& c, EdgeSide side);

    int                   m_width;
    int                   m_depth;
    std::vector<Tile>     m_tiles;  // level, then z, then x
    std::vector<EdgeKind> m_edgesX; // level, then z, then x + 1: edge between x and x + 1
    std::vector<EdgeKind> m_edgesZ; // level, then z + 1, then x: edge between z and z + 1
    std::vector<Stair>    m_stairs;
    uint64_t              m_revision = 1;
};
