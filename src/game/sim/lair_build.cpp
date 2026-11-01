#include "game_state.h"
#include "lair_sim.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

// What may be placed and deleted, and the placing and deleting. The job queue that then builds it
// is in lair_sim.cpp.

namespace
{
    constexpr WorldLevel kLevel = WorldLevel::Underground;

    std::string Money(int amount)
    {
        char text[32];
        snprintf(text, sizeof(text), "$%d", amount);
        return text;
    }

    LairCheck Refuse(std::string reason)
    {
        LairCheck check;
        check.ok     = false;
        check.reason = std::move(reason);
        return check;
    }

    size_t ReachIndex(const WorldGrid& world, const TileCoord& c)
    {
        return ((size_t)c.level * (size_t)world.GetDepth() + (size_t)c.z) * (size_t)world.GetWidth() + (size_t)c.x;
    }

    // Marks what's walkable from the foot of the stairs, on both levels.
    void Flood(const WorldGrid& world, std::vector<uint8_t>& reach)
    {
        const int width = world.GetWidth(), depth = world.GetDepth();
        reach.assign((size_t)kWorldLevelCount * (size_t)width * (size_t)depth, 0);
        std::vector<TileCoord> open;
        const auto visit = [&](const TileCoord& c) {
            uint8_t& seen = reach[ReachIndex(world, c)];
            if (!seen)
            {
                seen = 1;
                open.push_back(c);
            }
        };
        for (const Stair& stair : world.GetStairs())
            if (world.IsWalkable(stair.lower))
                visit(stair.lower);

        static constexpr int kSteps[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
        while (!open.empty())
        {
            const TileCoord c = open.back();
            open.pop_back();
            for (const auto& step : kSteps)
                if (world.CanStep(c, step[0], step[1]))
                    visit({ c.level, c.x + step[0], c.z + step[1] });
            for (const Stair& stair : world.GetStairs())
            {
                if (stair.lower == c && world.IsWalkable(stair.upper))
                    visit(stair.upper);
                else if (stair.upper == c && world.IsWalkable(stair.lower))
                    visit(stair.lower);
            }
        }
    }

    // Calls f(edge) for each edge on a rectangle's border.
    template <typename F>
    void ForEachBorderEdge(const LairRect& r, F&& f)
    {
        for (int x = r.x0; x <= r.x1; ++x)
        {
            f(MakeLairEdge({ kLevel, x, r.z0 }, EdgeSide::NegZ));
            f(MakeLairEdge({ kLevel, x, r.z1 }, EdgeSide::PosZ));
        }
        for (int z = r.z0; z <= r.z1; ++z)
        {
            f(MakeLairEdge({ kLevel, r.x0, z }, EdgeSide::NegX));
            f(MakeLairEdge({ kLevel, r.x1, z }, EdgeSide::PosX));
        }
    }

    void SetEdge(WorldGrid& world, const LairEdge& edge, EdgeKind kind)
    {
        world.SetEdge(edge.tile, edge.side, kind);
    }

    EdgeKind GetEdge(const WorldGrid& world, const LairEdge& edge)
    {
        return world.GetEdge(edge.tile, edge.side);
    }

    // The world as it will be when every job in the queue is done, which is what new things are
    // judged against.
    WorldGrid ProjectWorld(const GameState& state)
    {
        WorldGrid world = state.world;
        for (const LairRoom& room : state.lair.rooms)
            if (!room.built)
                ForEachBorderEdge(room.rect, [&](const LairEdge& edge) {
                    if (GetEdge(world, edge) == EdgeKind::None)
                        SetEdge(world, edge, EdgeKind::Wall);
                });
        for (const LairDoor& door : state.lair.doors)
            if (!door.built)
                SetEdge(world, door.edge, EdgeKind::Door);
        for (const LairObject& object : state.lair.objects)
        {
            const LairObjectType& type = GetLairObjectTypes()[(size_t)object.type];
            if (!object.built && type.solid)
                for (const TileCoord& tile : GetLairObjectTiles(type, object.tile, object.rotation))
                    world.SetTileSolid(tile, true);
        }
        return world;
    }

    // What `after` cuts off from the stairs compared with `before`, in words; empty if nothing.
    // Tiles of `except` don't count.
    std::string DescribeCutOff(const GameState& state, const WorldGrid& before, const WorldGrid& after, const LairRect* except)
    {
        std::vector<uint8_t> was, now;
        Flood(before, was);
        Flood(after, now);

        const auto lost = [&](const TileCoord& c) {
            return was[ReachIndex(before, c)] && !now[ReachIndex(before, c)] && !(except && except->Contains(c.x, c.z));
        };

        for (const CrewMember& member : state.crew)
        {
            if (member.away || member.motion.level != kLevel)
                continue;
            const TileCoord tile = state.world.WorldToTile(kLevel, member.motion.position);
            if (state.world.InBounds(tile) && lost(tile))
                return "Would trap " + member.name;
        }
        for (const LairObject& object : state.lair.objects)
        {
            const LairObjectType& type = GetLairObjectTypes()[(size_t)object.type];
            if (!type.solid)
            {
                if (lost(object.tile))
                    return std::string("Would cut off the ") + type.name;
                continue;
            }
            // A solid object is reached from the tiles around it.
            bool reachedBefore = false, reachedAfter = false;
            for (const TileCoord& tile : GetLairObjectTiles(type, object.tile, object.rotation))
            {
                for (const auto& step : { std::pair{ 1, 0 }, std::pair{ -1, 0 }, std::pair{ 0, 1 }, std::pair{ 0, -1 } })
                {
                    const TileCoord next = { kLevel, tile.x + step.first, tile.z + step.second };
                    if (!before.InBounds(next))
                        continue;
                    reachedBefore = reachedBefore || was[ReachIndex(before, next)];
                    reachedAfter  = reachedAfter || now[ReachIndex(before, next)];
                }
            }
            if (reachedBefore && !reachedAfter && !(except && except->Contains(object.tile.x, object.tile.z)))
                return std::string("Would cut off the ") + type.name;
        }
        for (const LairRoom& room : state.lair.rooms)
            for (int z = room.rect.z0; z <= room.rect.z1; ++z)
                for (int x = room.rect.x0; x <= room.rect.x1; ++x)
                    if (lost({ kLevel, x, z }))
                        return std::string("Would cut off the ") + GetLairRoomTypes()[(size_t)room.type].name;
        for (int z = 0; z < before.GetDepth(); ++z)
            for (int x = 0; x < before.GetWidth(); ++x)
                if (lost({ kLevel, x, z }))
                    return "Would cut off part of the floor from the stairs";
        return {};
    }

    bool IsStairTile(const WorldGrid& world, int x, int z)
    {
        for (const Stair& stair : world.GetStairs())
        {
            if ((stair.lower.x == x && stair.lower.z == z) || (stair.upper.x == x && stair.upper.z == z))
                return true;
        }
        return false;
    }

    int FindTypeIndexByKey(std::span<const LairRoomType> types, const char* key)
    {
        for (size_t i = 0; i < types.size(); ++i)
            if (strcmp(types[i].key, key) == 0)
                return (int)i;
        return -1;
    }

    void RemoveJob(GameState& state, LairJobKind kind, int target)
    {
        std::erase_if(state.lair.jobs, [&](const LairJob& job) { return job.kind == kind && job.target == target; });
    }

    // Whether the edge is still needed once `room` is gone: the other side is another room.
    bool OtherRoomUsesEdge(const GameState& state, const LairRoom& room, const LairEdge& edge)
    {
        TileCoord a, b;
        GetLairEdgeTiles(edge, a, b);
        const uint16_t roomA = state.world.GetTile(a).room;
        const uint16_t roomB = state.world.GetTile(b).room;
        return (roomA != 0 && roomA != room.id) || (roomB != 0 && roomB != room.id);
    }

    int RefundOf(int paid, bool built)
    {
        return built ? (int)((float)paid * kLairRefundFraction) : paid;
    }
}

// ---- Reading ----

const LairRoom* FindLairRoom(const GameState& state, uint16_t id)
{
    for (const LairRoom& room : state.lair.rooms)
        if (room.id == id)
            return &room;
    return nullptr;
}

const LairRoom* FindLairRoomAt(const GameState& state, const TileCoord& tile)
{
    if (tile.level != kLevel)
        return nullptr;
    const uint16_t id = state.world.GetTile(tile).room;
    return id ? FindLairRoom(state, id) : nullptr;
}

const LairObject* FindLairObjectAt(const GameState& state, const TileCoord& tile)
{
    for (const LairObject& object : state.lair.objects)
    {
        const LairObjectType& type = GetLairObjectTypes()[(size_t)object.type];
        const int width = object.rotation % 2 ? type.depth : type.width;
        const int depth = object.rotation % 2 ? type.width : type.depth;
        if (tile.x >= object.tile.x && tile.x < object.tile.x + width && tile.z >= object.tile.z && tile.z < object.tile.z + depth)
            return &object;
    }
    return nullptr;
}

const LairDoor* FindLairDoor(const GameState& state, const LairEdge& edge)
{
    for (const LairDoor& door : state.lair.doors)
        if (door.edge == edge)
            return &door;
    return nullptr;
}

const LairJob* FindLairDigJob(const GameState& state, const TileCoord& tile)
{
    for (const LairJob& job : state.lair.jobs)
        if (job.kind == LairJobKind::Dig && job.tile == tile)
            return &job;
    return nullptr;
}

const LairJob* FindLairJobFor(const GameState& state, LairJobKind kind, int target)
{
    for (const LairJob& job : state.lair.jobs)
        if (job.kind == kind && job.target == target)
            return &job;
    return nullptr;
}

bool IsLairTileReachable(const GameState& state, const TileCoord& tile)
{
    if (!state.world.InBounds(tile))
        return false;
    const size_t index = ReachIndex(state.world, tile);
    return index < state.lair.reachable.size() && state.lair.reachable[index];
}

bool IsLairDigReachable(const GameState& state, const TileCoord& tile)
{
    const auto open = [&](int dx, int dz, EdgeSide side) {
        const TileCoord next = { tile.level, tile.x + dx, tile.z + dz };
        return state.world.IsWalkable(next) && IsLairTileReachable(state, next) && state.world.GetEdge(tile, side) != EdgeKind::Wall;
    };
    return open(1, 0, EdgeSide::PosX) || open(-1, 0, EdgeSide::NegX) || open(0, 1, EdgeSide::PosZ) || open(0, -1, EdgeSide::NegZ);
}

void RefreshLairReachable(GameState& state)
{
    if (state.lair.reachableRevision == state.world.GetRevision() && !state.lair.reachable.empty())
        return;
    Flood(state.world, state.lair.reachable);
    state.lair.reachableRevision = state.world.GetRevision();
}

LairRoomStatus GetLairRoomStatus(const GameState& state, const LairRoom& room)
{
    const LairRoomType& type = GetLairRoomTypes()[(size_t)room.type];

    LairRoomStatus status;
    status.built = room.built;
    const int shortSide = std::min(room.rect.Width(), room.rect.Depth());
    const int longSide  = std::max(room.rect.Width(), room.rect.Depth());
    status.bigEnough = shortSide >= type.minShort && longSide >= type.minLong;

    ForEachBorderEdge(room.rect, [&](const LairEdge& edge) {
        if (GetEdge(state.world, edge) == EdgeKind::Door)
            status.hasDoor = true;
    });
    for (int z = room.rect.z0; z <= room.rect.z1 && !status.reachable; ++z)
        for (int x = room.rect.x0; x <= room.rect.x1 && !status.reachable; ++x)
            status.reachable = IsLairTileReachable(state, { kLevel, x, z });

    status.hasObjects = true;
    std::string missing;
    for (const LairRequirement& need : type.required)
    {
        if (!need.object)
            continue;
        int have = 0;
        const LairObjectType* needed = nullptr;
        for (const LairObjectType& objectType : GetLairObjectTypes())
            if (strcmp(objectType.key, need.object) == 0)
                needed = &objectType;
        for (const LairObject& object : state.lair.objects)
            if (object.room == room.id && object.built && &GetLairObjectTypes()[(size_t)object.type] == needed)
                ++have;
        if (have < need.count && missing.empty())
        {
            status.hasObjects = false;
            missing = std::string("Needs a ") + (needed ? needed->name : need.object);
        }
    }

    if (!status.built)
        status.problem = "Being built";
    else if (!status.bigEnough)
        status.problem = "Too small";
    else if (!status.hasDoor)
        status.problem = "Needs a door";
    else if (!status.reachable)
        status.problem = "Its door leads nowhere";
    else if (!status.hasObjects)
        status.problem = missing;
    status.working = status.problem.empty();
    return status;
}

std::vector<TileCoord> GetLairObjectTiles(const LairObjectType& type, const TileCoord& tile, int rotation)
{
    const int width = rotation % 2 ? type.depth : type.width;
    const int depth = rotation % 2 ? type.width : type.depth;
    std::vector<TileCoord> tiles;
    tiles.reserve((size_t)(width * depth));
    for (int z = 0; z < depth; ++z)
        for (int x = 0; x < width; ++x)
            tiles.push_back({ tile.level, tile.x + x, tile.z + z });
    return tiles;
}

Vec3 GetLairObjectCenter(const GameState& state, const LairObjectType& type, const TileCoord& tile, int rotation)
{
    const int  width = rotation % 2 ? type.depth : type.width;
    const int  depth = rotation % 2 ? type.width : type.depth;
    const Vec3 first = state.world.TileCenter(tile);
    return first + Vec3{ 0.5f * kTileSize * (float)(width - 1), 0.0f, 0.5f * kTileSize * (float)(depth - 1) };
}

LairEdge MakeLairEdge(const TileCoord& tile, EdgeSide side)
{
    switch (side)
    {
    case EdgeSide::NegX: return { { tile.level, tile.x - 1, tile.z }, EdgeSide::PosX };
    case EdgeSide::NegZ: return { { tile.level, tile.x, tile.z - 1 }, EdgeSide::PosZ };
    default:             return { tile, side };
    }
}

void GetLairEdgeTiles(const LairEdge& edge, TileCoord& a, TileCoord& b)
{
    a = edge.tile;
    b = edge.side == EdgeSide::PosX ? TileCoord{ a.level, a.x + 1, a.z } : TileCoord{ a.level, a.x, a.z + 1 };
}

Vec3 GetLairEdgeCenter(const GameState& state, const LairEdge& edge)
{
    const Vec3 center = state.world.TileCenter(edge.tile);
    return center + (edge.side == EdgeSide::PosX ? Vec3{ 0.5f * kTileSize, 0.0f, 0.0f } : Vec3{ 0.0f, 0.0f, 0.5f * kTileSize });
}

int CrewCapacity(const GameState&)
{
    return kCrewLimit;
}

int ExtraCrateCapacity(const GameState& state)
{
    int capacity = 0;
    for (const LairObject& object : state.lair.objects)
    {
        const LairObjectType& type = GetLairObjectTypes()[(size_t)object.type];
        const LairRoom*       room = FindLairRoom(state, object.room);
        if (object.built && type.crateCapacity > 0 && room && GetLairRoomStatus(state, *room).working)
            capacity += type.crateCapacity;
    }
    return capacity;
}

int64_t ProtectedMoney(const GameState& state)
{
    int64_t money = 0;
    for (const LairObject& object : state.lair.objects)
    {
        const LairObjectType& type = GetLairObjectTypes()[(size_t)object.type];
        const LairRoom*       room = FindLairRoom(state, object.room);
        if (object.built && type.protectedMoney > 0 && room && GetLairRoomStatus(state, *room).working)
            money += type.protectedMoney;
    }
    return std::min(money, std::max<int64_t>(state.money, 0));
}

// ---- Digging ----

LairCheck CheckLairDig(const GameState& state, const TileCoord& tile)
{
    const WorldGrid& world = state.world;
    // The outermost ring stays rock, so walls never lie on the grid's border.
    if (tile.level != kLevel || tile.x < 1 || tile.z < 1 || tile.x >= world.GetWidth() - 1 || tile.z >= world.GetDepth() - 1)
        return Refuse("Too close to the edge of the property");
    if (world.GetTile(tile).kind != TileKind::Earth)
        return Refuse("Already dug");
    if (IsStairTile(world, tile.x, tile.z))
        return Refuse("The stairs run through here");
    if (FindLairDigJob(state, tile))
        return Refuse("Already marked");

    LairCheck check;
    check.cost = kLairDigCost;
    if (state.money < check.cost)
        return Refuse("Not enough money");
    return check;
}

bool MarkLairDig(GameState& state, const TileCoord& tile)
{
    const LairCheck check = CheckLairDig(state, tile);
    if (!check.ok)
        return false;
    state.money -= check.cost;

    LairJob job;
    job.id       = state.lair.nextJobId++;
    job.kind     = LairJobKind::Dig;
    job.tile     = tile;
    job.duration = kLairDigSeconds;
    job.paid     = check.cost;
    state.lair.jobs.push_back(job);
    return true;
}

bool CancelLairDig(GameState& state, const TileCoord& tile)
{
    const LairJob* job = FindLairDigJob(state, tile);
    if (!job)
        return false;
    state.money += job->paid;
    std::erase_if(state.lair.jobs, [&](const LairJob& other) { return other.kind == LairJobKind::Dig && other.tile == tile; });
    return true;
}

// ---- Rooms ----

LairCheck CheckLairRoom(const GameState& state, int type, const LairRect& rect)
{
    const LairRoomType& def   = GetLairRoomTypes()[(size_t)type];
    const WorldGrid&    world = state.world;

    for (int z = rect.z0; z <= rect.z1; ++z)
    {
        for (int x = rect.x0; x <= rect.x1; ++x)
        {
            const Tile& tile = world.GetTile({ kLevel, x, z });
            if (tile.room != 0)
                return Refuse("Overlaps another room");
            if (tile.kind == TileKind::Stairwell)
                return Refuse("The stairs are in the way");
            if (tile.kind != TileKind::Floor)
                return Refuse("Rooms go on dug floor: dig here first");
            if (IsStairTile(world, x, z))
                return Refuse("Keep the foot of the stairs clear");
        }
    }

    const int shortSide = std::min(rect.Width(), rect.Depth());
    const int longSide  = std::max(rect.Width(), rect.Depth());
    if (shortSide < def.minShort || longSide < def.minLong)
    {
        char text[96];
        snprintf(text, sizeof(text), "%s needs at least %d x %d tiles", def.name, def.minShort, def.minLong);
        return Refuse(text);
    }

    if (def.unique)
        for (const LairRoom& room : state.lair.rooms)
            if (room.type == type)
                return Refuse(std::string("You can only have one ") + def.name);

    // What it costs: its tiles and the walls that aren't there yet.
    const WorldGrid before = ProjectWorld(state);
    WorldGrid       after  = before;
    int             walls  = 0;
    ForEachBorderEdge(rect, [&](const LairEdge& edge) {
        if (GetEdge(before, edge) == EdgeKind::None)
        {
            SetEdge(after, edge, EdgeKind::Wall);
            ++walls;
        }
    });

    LairCheck check;
    check.cost = rect.Width() * rect.Depth() * def.costPerTile + walls * kLairWallCost;

    // The builders have to get to it, and it mustn't cut anything else off.
    std::vector<uint8_t> reach;
    Flood(before, reach);
    bool reachable = false;
    for (int z = rect.z0; z <= rect.z1 && !reachable; ++z)
        for (int x = rect.x0; x <= rect.x1 && !reachable; ++x)
            reachable = reach[ReachIndex(world, { kLevel, x, z })] != 0;
    if (!reachable)
        return Refuse("Nobody can get there");

    const std::string cutOff = DescribeCutOff(state, before, after, &rect);
    if (!cutOff.empty())
        return Refuse(cutOff);

    if (state.money < check.cost)
        return Refuse("Not enough money: costs " + Money(check.cost));
    return check;
}

LairCheck PlaceLairRoom(GameState& state, int type, const LairRect& rect)
{
    const LairCheck check = CheckLairRoom(state, type, rect);
    if (!check.ok)
        return check;
    state.money -= check.cost;

    LairRoom room;
    room.id   = state.lair.nextRoomId++;
    room.type = type;
    room.rect = rect;
    room.paid = check.cost;
    state.lair.rooms.push_back(room);

    int walls = 0;
    ForEachBorderEdge(rect, [&](const LairEdge& edge) {
        if (GetEdge(state.world, edge) == EdgeKind::None)
            ++walls;
    });
    for (int z = rect.z0; z <= rect.z1; ++z)
        for (int x = rect.x0; x <= rect.x1; ++x)
            state.world.SetTileRoom({ kLevel, x, z }, room.id);

    LairJob job;
    job.id         = state.lair.nextJobId++;
    job.kind       = LairJobKind::Room;
    job.target     = room.id;
    job.duration   = kLairRoomSeconds + kLairRoomWallSeconds * (float)walls;
    job.paid       = check.cost;
    job.maxWorkers = kLairRoomBuilders;
    state.lair.jobs.push_back(job);
    return check;
}

void RaiseLairRoomWalls(GameState& state, const LairRoom& room)
{
    ForEachBorderEdge(room.rect, [&](const LairEdge& edge) {
        if (GetEdge(state.world, edge) == EdgeKind::None)
            SetEdge(state.world, edge, EdgeKind::Wall);
    });
}

LairCheck CheckDeleteLairRoom(const GameState& state, const LairRoom& room)
{
    LairCheck check;
    check.cost = RefundOf(room.paid, room.built);
    // The objects and doors go too.
    for (const LairObject& object : state.lair.objects)
        if (object.room == room.id)
            check.cost += RefundOf(object.paid, object.built);
    ForEachBorderEdge(room.rect, [&](const LairEdge& edge) {
        if (const LairDoor* door = FindLairDoor(state, edge))
            if (!OtherRoomUsesEdge(state, room, edge))
                check.cost += RefundOf(kLairDoorCost, door->built);
    });
    return check;
}

LairCheck DeleteLairRoom(GameState& state, uint16_t roomId)
{
    const LairRoom* found = FindLairRoom(state, roomId);
    if (!found)
        return Refuse("No such room");
    const LairRoom room  = *found;
    LairCheck      check = CheckDeleteLairRoom(state, room);
    state.money += check.cost;

    // Its objects go, solid or not.
    for (const LairObject& object : state.lair.objects)
    {
        if (object.room != room.id)
            continue;
        RemoveJob(state, LairJobKind::Object, object.id);
        const LairObjectType& type = GetLairObjectTypes()[(size_t)object.type];
        if (object.built && type.solid)
            for (const TileCoord& tile : GetLairObjectTiles(type, object.tile, object.rotation))
                state.world.SetTileSolid(tile, false);
    }
    std::erase_if(state.lair.objects, [&](const LairObject& object) { return object.room == room.id; });

    ForEachBorderEdge(room.rect, [&](const LairEdge& edge) {
        const LairDoor* door = FindLairDoor(state, edge);
        if (OtherRoomUsesEdge(state, room, edge))
            return; // a wall both rooms have, and any door in it, stays
        if (door)
        {
            RemoveJob(state, LairJobKind::Door, door->id);
            const int id = door->id;
            std::erase_if(state.lair.doors, [&](const LairDoor& other) { return other.id == id; });
        }
        if (room.built || GetEdge(state.world, edge) == EdgeKind::Door)
            SetEdge(state.world, edge, EdgeKind::None);
    });
    for (int z = room.rect.z0; z <= room.rect.z1; ++z)
        for (int x = room.rect.x0; x <= room.rect.x1; ++x)
            state.world.SetTileRoom({ kLevel, x, z }, 0);

    RemoveJob(state, LairJobKind::Room, room.id);
    std::erase_if(state.lair.rooms, [&](const LairRoom& other) { return other.id == room.id; });
    RefreshLairReachable(state);
    return check;
}

// ---- Doors ----

LairCheck CheckLairDoor(const GameState& state, const LairEdge& edge)
{
    TileCoord a, b;
    GetLairEdgeTiles(edge, a, b);
    if (!state.world.InBounds(a) || !state.world.InBounds(b))
        return Refuse("Doors go on a wall");

    const Tile& tileA = state.world.GetTile(a);
    const Tile& tileB = state.world.GetTile(b);
    if (FindLairDoor(state, edge))
        return Refuse("There's a door here already");
    // A wall that's up, or one a room in the queue will raise.
    const bool planned = (FindLairRoomAt(state, a) && !FindLairRoomAt(state, a)->built && tileA.room != tileB.room) ||
                         (FindLairRoomAt(state, b) && !FindLairRoomAt(state, b)->built && tileA.room != tileB.room);
    if (GetEdge(state.world, edge) != EdgeKind::Wall && !planned)
        return Refuse("Doors go on a wall");
    if (tileA.kind == TileKind::Stairwell || tileB.kind == TileKind::Stairwell)
        return Refuse("The stairs are in the way");
    if (tileA.solid || tileB.solid || (FindLairObjectAt(state, a) && GetLairObjectTypes()[(size_t)FindLairObjectAt(state, a)->type].solid) ||
        (FindLairObjectAt(state, b) && GetLairObjectTypes()[(size_t)FindLairObjectAt(state, b)->type].solid))
        return Refuse("Something stands in front of it");

    LairCheck check;
    check.cost = kLairDoorCost;
    if (state.money < check.cost)
        return Refuse("Not enough money: costs " + Money(check.cost));
    return check;
}

LairCheck PlaceLairDoor(GameState& state, const LairEdge& edge)
{
    const LairCheck check = CheckLairDoor(state, edge);
    if (!check.ok)
        return check;
    state.money -= check.cost;

    LairDoor door;
    door.id   = state.lair.nextDoorId++;
    door.edge = edge;
    state.lair.doors.push_back(door);

    LairJob job;
    job.id       = state.lair.nextJobId++;
    job.kind     = LairJobKind::Door;
    job.target   = door.id;
    job.duration = kLairDoorSeconds;
    job.paid     = check.cost;
    state.lair.jobs.push_back(job);
    return check;
}

LairCheck CheckDeleteLairDoor(const GameState& state, const LairDoor& door)
{
    LairCheck check;
    check.cost = RefundOf(kLairDoorCost, door.built);
    if (!door.built)
        return check; // only cancels the job

    // It would leave its room with no door.
    TileCoord a, b;
    GetLairEdgeTiles(door.edge, a, b);
    for (const TileCoord& side : { a, b })
    {
        const LairRoom* room = FindLairRoomAt(state, side);
        if (!room)
            continue;
        int count = 0;
        ForEachBorderEdge(room->rect, [&](const LairEdge& edge) {
            if (edge != door.edge && (GetEdge(state.world, edge) == EdgeKind::Door || FindLairDoor(state, edge)))
                ++count;
        });
        if (count == 0)
            return Refuse(std::string("It's the ") + GetLairRoomTypes()[(size_t)room->type].name + "'s only door");
    }

    // It would trap someone or something.
    const WorldGrid before = ProjectWorld(state);
    WorldGrid       after  = before;
    SetEdge(after, door.edge, EdgeKind::Wall);
    const std::string cutOff = DescribeCutOff(state, before, after, nullptr);
    if (!cutOff.empty())
        return Refuse(cutOff);
    return check;
}

LairCheck DeleteLairDoor(GameState& state, int doorId)
{
    const auto found = std::find_if(state.lair.doors.begin(), state.lair.doors.end(), [&](const LairDoor& door) { return door.id == doorId; });
    if (found == state.lair.doors.end())
        return Refuse("No such door");
    const LairDoor  door  = *found;
    const LairCheck check = CheckDeleteLairDoor(state, door);
    if (!check.ok)
        return check;

    state.money += check.cost;
    RemoveJob(state, LairJobKind::Door, door.id);
    if (door.built)
        SetEdge(state.world, door.edge, EdgeKind::Wall);
    state.lair.doors.erase(found);
    RefreshLairReachable(state);
    return check;
}

// ---- Objects ----

LairCheck CheckLairObject(const GameState& state, int type, const TileCoord& tile, int rotation)
{
    const LairObjectType& def   = GetLairObjectTypes()[(size_t)type];
    const int             needs = FindTypeIndexByKey(GetLairRoomTypes(), def.room);
    const char*           room  = needs >= 0 ? GetLairRoomTypes()[(size_t)needs].name : def.room;

    const std::vector<TileCoord> tiles = GetLairObjectTiles(def, tile, rotation);
    uint16_t                     roomId = 0;
    for (const TileCoord& c : tiles)
    {
        const LairRoom* found = FindLairRoomAt(state, c);
        if (!found || found->type != needs)
            return Refuse(std::string(def.name) + " goes in a " + room + " room");
        if (roomId != 0 && roomId != found->id)
            return Refuse("It has to fit in one room");
        roomId = found->id;
        if (FindLairObjectAt(state, c))
            return Refuse("Something's already there");
    }

    if (def.solid)
    {
        // It mustn't block a door, or cut anyone or anything off.
        const WorldGrid before = ProjectWorld(state);
        WorldGrid       after  = before;
        for (const TileCoord& c : tiles)
            after.SetTileSolid(c, true);

        LairRect footprint = { tiles.front().x, tiles.front().z, tiles.back().x, tiles.back().z };
        for (const LairDoor& door : state.lair.doors)
        {
            TileCoord a, b;
            GetLairEdgeTiles(door.edge, a, b);
            if (footprint.Contains(a.x, a.z) || footprint.Contains(b.x, b.z))
                return Refuse("It would block a door");
        }
        std::string cutOff = DescribeCutOff(state, before, after, &footprint);
        if (!cutOff.empty())
            return Refuse(cutOff);

        // Someone has to be able to walk up to it. Without a door yet, that's judged later.
        const LairRoom* placed = FindLairRoom(state, roomId);
        std::vector<uint8_t> reach;
        Flood(after, reach);
        bool roomReachable = false;
        for (int z = placed->rect.z0; z <= placed->rect.z1 && !roomReachable; ++z)
            for (int x = placed->rect.x0; x <= placed->rect.x1 && !roomReachable; ++x)
                roomReachable = reach[ReachIndex(after, { kLevel, x, z })] != 0;
        bool free = false;
        for (const TileCoord& c : tiles)
            for (const auto& step : { std::pair{ 1, 0 }, std::pair{ -1, 0 }, std::pair{ 0, 1 }, std::pair{ 0, -1 } })
            {
                const TileCoord next = { kLevel, c.x + step.first, c.z + step.second };
                if (after.InBounds(next) && reach[ReachIndex(after, next)])
                    free = true;
            }
        if (roomReachable && !free)
            return Refuse("Nobody could get to it");
    }

    LairCheck check;
    check.cost = def.cost;
    if (state.money < check.cost)
        return Refuse("Not enough money: costs " + Money(check.cost));
    return check;
}

LairCheck PlaceLairObject(GameState& state, int type, const TileCoord& tile, int rotation)
{
    const LairCheck check = CheckLairObject(state, type, tile, rotation);
    if (!check.ok)
        return check;
    state.money -= check.cost;

    const LairObjectType& def = GetLairObjectTypes()[(size_t)type];
    LairObject object;
    object.id       = state.lair.nextObjectId++;
    object.type     = type;
    object.tile     = tile;
    object.rotation = rotation;
    object.room     = state.world.GetTile(tile).room;
    object.paid     = check.cost;
    state.lair.objects.push_back(object);

    LairJob job;
    job.id       = state.lair.nextJobId++;
    job.kind     = LairJobKind::Object;
    job.target   = object.id;
    job.duration = def.buildSeconds;
    job.paid     = check.cost;
    state.lair.jobs.push_back(job);
    return check;
}

LairCheck CheckDeleteLairObject(const GameState& state, const LairObject& object)
{
    (void)state;
    LairCheck check;
    check.cost = RefundOf(object.paid, object.built);
    return check;
}

void RemoveLairObjectForRaid(GameState& state, int objectId)
{
    // DeleteLairObject, with its refund taken back.
    const int64_t money = state.money;
    DeleteLairObject(state, objectId);
    state.money = money;
}

LairCheck DeleteLairObject(GameState& state, int objectId)
{
    const auto found = std::find_if(state.lair.objects.begin(), state.lair.objects.end(), [&](const LairObject& object) { return object.id == objectId; });
    if (found == state.lair.objects.end())
        return Refuse("No such object");
    const LairCheck check = CheckDeleteLairObject(state, *found);

    state.money += check.cost;
    RemoveJob(state, LairJobKind::Object, objectId);
    const LairObjectType& type = GetLairObjectTypes()[(size_t)found->type];
    if (found->built && type.solid)
        for (const TileCoord& tile : GetLairObjectTiles(type, found->tile, found->rotation))
            state.world.SetTileSolid(tile, false);
    state.lair.objects.erase(found);
    RefreshLairReachable(state);
    return check;
}

bool CancelLairJobAt(GameState& state, const TileCoord& tile)
{
    if (CancelLairDig(state, tile))
        return true;
    if (const LairObject* object = FindLairObjectAt(state, tile))
    {
        if (!object->built)
        {
            DeleteLairObject(state, object->id);
            return true;
        }
    }
    if (const LairRoom* room = FindLairRoomAt(state, tile))
    {
        if (!room->built)
        {
            DeleteLairRoom(state, room->id);
            return true;
        }
    }
    return false;
}
