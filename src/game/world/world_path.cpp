#include "world_path.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>

namespace
{
    constexpr float kLineStep = 0.1f;  // HasClearLine sampling distance, world units
    constexpr float kDiagonal = 1.41421356f;
    constexpr float kStairCost = (float)kStairSpanTiles + 1.0f; // tiles; a bit dearer than walking the distance

    struct Neighbor
    {
        int   dx, dz;
        float cost;
    };
    constexpr Neighbor kNeighbors[8] = {
        { 1, 0, 1.0f }, { -1, 0, 1.0f }, { 0, 1, 1.0f }, { 0, -1, 1.0f },
        { 1, 1, kDiagonal }, { 1, -1, kDiagonal }, { -1, 1, kDiagonal }, { -1, -1, kDiagonal },
    };

    // Octile distance: exact on an empty 8-connected grid, so A* stays optimal.
    float Octile(const TileCoord& a, const TileCoord& b)
    {
        const float dx = (float)std::abs(b.x - a.x);
        const float dz = (float)std::abs(b.z - a.z);
        return std::max(dx, dz) + (kDiagonal - 1.0f) * std::min(dx, dz);
    }

    // Never overestimates: on the same level the straight distance, across levels the cheapest
    // straight route through any staircase.
    float Heuristic(const WorldGrid& grid, const TileCoord& a, const TileCoord& b)
    {
        if (a.level == b.level)
            return Octile(a, b);

        float best = 1e30f;
        for (const Stair& stair : grid.GetStairs())
        {
            const TileCoord& onA = a.level == WorldLevel::Ground ? stair.upper : stair.lower;
            const TileCoord& onB = a.level == WorldLevel::Ground ? stair.lower : stair.upper;
            best = std::min(best, Octile(a, onA) + kStairCost + Octile(onB, b));
        }
        return best;
    }

    // The stair linking two tiles on different levels.
    const Stair* FindStair(const WorldGrid& grid, const TileCoord& a, const TileCoord& b)
    {
        for (const Stair& stair : grid.GetStairs())
            if ((stair.upper == a && stair.lower == b) || (stair.upper == b && stair.lower == a))
                return &stair;
        return nullptr;
    }
}

bool HasClearLine(const WorldGrid& grid, WorldLevel level, Vec3 a, Vec3 b, float radius)
{
    const float dx     = b.x - a.x;
    const float dz     = b.z - a.z;
    const float length = std::sqrt(dx * dx + dz * dz);
    const Vec3  side   = length > 1e-5f ? Vec3{ -dz / length, 0.0f, dx / length } * radius : Vec3{};
    const int   steps  = std::max(1, (int)std::ceil(length / kLineStep));

    for (const float offset : { 0.0f, 1.0f, -1.0f })
    {
        const Vec3 shift    = side * offset;
        TileCoord  previous = grid.WorldToTile(level, a + shift);
        if (!grid.IsWalkable(previous))
            return false;
        for (int i = 1; i <= steps; ++i)
        {
            const Vec3      p    = a + shift + Vec3{ dx, 0.0f, dz } * ((float)i / (float)steps);
            const TileCoord tile = grid.WorldToTile(level, p);
            if (tile == previous)
                continue;
            if (!grid.CanStep(previous, tile.x - previous.x, tile.z - previous.z))
                return false;
            previous = tile;
        }
    }
    return true;
}

bool FindNearestWalkable(const WorldGrid& grid, const TileCoord& around, int maxRings, TileCoord& result)
{
    if (grid.IsWalkable(around))
    {
        result = around;
        return true;
    }
    for (int ring = 1; ring <= maxRings; ++ring)
    {
        float best = 1e30f;
        for (int z = around.z - ring; z <= around.z + ring; ++z)
        {
            for (int x = around.x - ring; x <= around.x + ring; ++x)
            {
                const TileCoord c = { around.level, x, z };
                if (std::max(std::abs(x - around.x), std::abs(z - around.z)) != ring || !grid.IsWalkable(c))
                    continue;
                const float d = Octile(around, c);
                if (d < best)
                {
                    best   = d;
                    result = c;
                }
            }
        }
        if (best < 1e30f)
            return true;
    }
    return false;
}

bool FindTilePath(const WorldGrid& grid, const TileCoord& from, const TileCoord& to, std::vector<TileCoord>& tiles)
{
    tiles.clear();
    if (!grid.IsWalkable(to) || !grid.InBounds(from))
        return false;
    if (from == to)
        return true;

    const int    width     = grid.GetWidth();
    const int    depth     = grid.GetDepth();
    const size_t nodeCount = (size_t)kWorldLevelCount * (size_t)width * (size_t)depth;
    auto indexOf = [&](const TileCoord& c) { return (int32_t)(((size_t)c.level * (size_t)depth + (size_t)c.z) * (size_t)width + (size_t)c.x); };
    auto coordOf = [&](int32_t i) {
        return TileCoord{ (WorldLevel)(i / (width * depth)), i % width, (i / width) % depth };
    };

    std::vector<float>   cost(nodeCount, 1e30f);
    std::vector<int32_t> parent(nodeCount, -1);
    std::vector<uint8_t> closed(nodeCount, 0);

    using Entry = std::pair<float, int32_t>; // (estimated total cost, node)
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;

    const int32_t start = indexOf(from);
    const int32_t goal  = indexOf(to);
    cost[(size_t)start] = 0.0f;
    open.push({ Heuristic(grid, from, to), start });

    auto relax = [&](int32_t current, const TileCoord& next, float stepCost) {
        const int32_t node    = indexOf(next);
        const float   newCost = cost[(size_t)current] + stepCost;
        if (newCost < cost[(size_t)node])
        {
            cost[(size_t)node]   = newCost;
            parent[(size_t)node] = current;
            open.push({ newCost + Heuristic(grid, next, to), node });
        }
    };

    bool reached = false;
    while (!open.empty())
    {
        const int32_t current = open.top().second;
        open.pop();
        if (closed[(size_t)current])
            continue;
        closed[(size_t)current] = 1;
        if (current == goal)
        {
            reached = true;
            break;
        }

        const TileCoord c = coordOf(current);
        for (const Neighbor& n : kNeighbors)
            if (grid.CanStep(c, n.dx, n.dz))
                relax(current, { c.level, c.x + n.dx, c.z + n.dz }, n.cost);
        for (const Stair& stair : grid.GetStairs())
        {
            if (stair.upper == c && grid.IsWalkable(stair.lower))
                relax(current, stair.lower, kStairCost);
            else if (stair.lower == c && grid.IsWalkable(stair.upper))
                relax(current, stair.upper, kStairCost);
        }
    }
    if (!reached)
        return false;

    for (int32_t n = goal; n != start; n = parent[(size_t)n])
        tiles.push_back(coordOf(n));
    std::reverse(tiles.begin(), tiles.end());
    return true;
}

bool FindWorldPath(const WorldGrid& grid, Vec3 position, const TileCoord& from, const TileCoord& to, float radius,
                   std::vector<WorldWaypoint>& path)
{
    path.clear();
    std::vector<TileCoord> tiles;
    if (!FindTilePath(grid, from, to, tiles))
        return false;
    if (tiles.empty())
    {
        path.push_back({ grid.TileCenter(to), to.level });
        return true;
    }

    Vec3      current = position;
    TileCoord tile    = from;
    size_t    i       = 0;
    while (i < tiles.size())
    {
        if (tiles[i].level != tile.level)
        {
            // Flat to the stair's end of the tile, down the stair, flat to the far tile's center.
            const Stair* stair = FindStair(grid, tile, tiles[i]);
            const Vec3   here  = grid.TileCenter(tile);
            const Vec3   there = grid.TileCenter(tiles[i]);
            const Vec3   flat  = Normalize({ there.x - here.x, 0.0f, there.z - here.z }) * (0.5f * kTileSize);
            if (stair)
            {
                path.push_back({ here, tile.level });
                path.push_back({ here + flat, tile.level });
                path.push_back({ there - flat, tiles[i].level });
            }
            path.push_back({ there, tiles[i].level });
            current = there;
            tile    = tiles[i];
            ++i;
            continue;
        }

        size_t runEnd = i;
        while (runEnd < tiles.size() && tiles[runEnd].level == tile.level)
            ++runEnd;

        // String pulling: from each point, jump to the farthest tile center in clear sight. A
        // tile before a staircase is kept as the run's end by the stair branch above.
        while (i < runEnd)
        {
            size_t farthest = i;
            for (size_t j = runEnd - 1; j > i; --j)
            {
                if (HasClearLine(grid, tile.level, current, grid.TileCenter(tiles[j]), radius))
                {
                    farthest = j;
                    break;
                }
            }
            current = grid.TileCenter(tiles[farthest]);
            path.push_back({ current, tile.level });
            i = farthest + 1;
        }
        tile = tiles[runEnd - 1];
    }
    return true;
}
