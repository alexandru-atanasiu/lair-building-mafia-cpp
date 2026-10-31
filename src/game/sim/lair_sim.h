#pragma once

#include "../world/world_grid.h"
#include "core/color.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

struct GameState;
struct SimStep;

// The lair: digging, rooms, room objects, doors, the job queue that builds them and what free crew
// do all day. Rules are here and in lair_build.cpp; the room and object tables in lair_tables.cpp;
// nothing here draws. Everything the player places is paid for at once and becomes a job; free crew
// (away == false && jobId == 0, never the boss) walk over and do it, nearest first.

// ---- Tables (lair_tables.cpp). A new room or object is one entry there. ----

constexpr int kLairMaxObjectParts = 4;

// One colored piece of an object's model. Meshes have one color per instance, so a thing of
// several colors is several meshes (py/create_meshes.py).
struct LairObjectPart
{
    const char* mesh = nullptr;
    Color       color;
};

struct LairObjectType
{
    const char* key;                // unique, what rooms refer to it by
    const char* name;
    const char* description;
    const char* icon;               // ui/icons/<name>.tex
    const char* room;               // the room type's key it can be placed in
    int         width = 1;          // footprint in tiles, along x before rotation
    int         depth = 1;          // along z before rotation
    int         cost = 0;
    float       buildSeconds = 4.0f;
    bool        solid = true;       // blocks walking
    float       height = 1.0f;      // metres, for picking with the mouse
    LairObjectPart parts[kLairMaxObjectParts];

    // What it does, while its room works.
    int   crewBeds = 0;             // adds to CrewCapacity
    int   crateCapacity = 0;        // adds to ExtraCrateCapacity
    int   protectedMoney = 0;       // adds to ProtectedMoney
    // How free crew use it: they stand beside it (or, a seat, on its tile) for a while.
    float useSeconds = 0.0f;        // 0 = crew don't use it
    bool  seat = false;             // not solid: the user stands on its own tile
    bool  bossSeat = false;         // the boss's place: he stays there, nobody else uses it
    // What an investigator who sees it reports (law_sim.h).
    float evidence = 0.0f;
};

struct LairRequirement
{
    const char* object = nullptr;   // an object type's key
    int         count = 1;
};

struct LairRoomType
{
    const char* key;
    const char* name;
    const char* description;
    const char* icon;
    int         minShort = 2;       // min size: short side and long side, in tiles
    int         minLong = 2;
    int         costPerTile = 10;
    bool        unique = false;     // one only
    Color       floorTint;
    LairRequirement required[2];    // objects it needs to work
};

std::span<const LairRoomType>   GetLairRoomTypes();
std::span<const LairObjectType> GetLairObjectTypes();

// ---- Tuning ----

constexpr int   kLairDigCost        = 25;   // per tile
constexpr float kLairDigSeconds     = 3.5f; // sim seconds per tile
constexpr int   kLairWallCost       = 10;   // per new wall edge of a room
constexpr float kLairRoomSeconds    = 2.0f; // a room: this, plus per wall below, shared by its builders
constexpr float kLairRoomWallSeconds = 0.6f;
constexpr int   kLairRoomBuilders   = 3;
constexpr int   kLairDoorCost       = 100;
constexpr float kLairDoorSeconds    = 3.0f;
constexpr float kLairRefundFraction = 0.5f; // of the cost, for deleting something built; cancelling a job refunds it all
constexpr int   kLairBaseCrewCapacity = 6;  // a new game's men sleep on the floor; anyone more needs a bunk
constexpr int   kLairFirstJobId     = 1000000; // far from anyone else's CrewMember::jobId

// ---- State ----

enum class LairJobKind : uint8_t
{
    Dig,
    Room,
    Door,
    Object,
};

// A rectangle of tiles on the underground level, inclusive.
struct LairRect
{
    int x0 = 0, z0 = 0, x1 = 0, z1 = 0;

    int  Width() const { return x1 - x0 + 1; }
    int  Depth() const { return z1 - z0 + 1; }
    bool Contains(int x, int z) const { return x >= x0 && x <= x1 && z >= z0 && z <= z1; }
};

struct LairRoom
{
    uint16_t id = 0;   // what the tiles' `room` holds
    int      type = 0; // index into GetLairRoomTypes()
    LairRect rect;
    bool     built = false; // its walls are up
    int      paid = 0;
};

// A wall edge in canonical form: PosX or PosZ of `tile`.
struct LairEdge
{
    TileCoord tile;
    EdgeSide  side = EdgeSide::PosX;

    friend bool operator==(const LairEdge&, const LairEdge&) = default;
};

struct LairDoor
{
    int      id = 0;
    LairEdge edge;
    bool     built = false;
};

struct LairObject
{
    int       id = 0;
    int       type = 0;  // index into GetLairObjectTypes()
    TileCoord tile;      // the footprint's lowest x and z
    int       rotation = 0; // quarter turns
    uint16_t  room = 0;
    bool      built = false;
    int       paid = 0;
};

struct LairJob
{
    int         id = 0;   // what a worker's CrewMember::jobId holds
    LairJobKind kind = LairJobKind::Dig;
    TileCoord   tile;     // Dig: the earth tile
    int         target = 0; // Room / Door / Object: its id
    float       progress = 0.0f; // work seconds done
    float       duration = 1.0f;
    int         paid = 0;
    int         maxWorkers = 1;
};

// What a crew member is up to in the lair, beyond their CrewMember.
struct LairCrewTask
{
    uint32_t  crewId = 0;
    bool      hasSpot = false;   // is headed for or standing at `spot`
    TileCoord spot;
    bool      faces = false;     // once there, turns toward `faceAt`
    Vec3      faceAt;
};

struct LairSim
{
    bool wanderEnabled = true;

    std::vector<LairRoom>     rooms;
    std::vector<LairDoor>     doors;
    std::vector<LairObject>   objects;
    std::vector<LairJob>      jobs;  // oldest first
    std::vector<LairCrewTask> tasks;

    uint16_t nextRoomId   = 1;
    int      nextDoorId   = 1;
    int      nextObjectId = 1;
    int      nextJobId    = kLairFirstJobId;
    float    dispatchTimer = 0.0f;

    // Tiles walkable from the foot of the stairs, on both levels. Kept current by Tick and by
    // everything in lair_build.cpp; see IsLairTileReachable.
    std::vector<uint8_t> reachable;
    uint64_t             reachableRevision = 0;

    void Tick(GameState& state, const SimStep& step);
};

// ---- Reading the lair (other systems: use these) ----

// Why a room does or doesn't work.
struct LairRoomStatus
{
    bool        working = false;
    bool        built = false;
    bool        bigEnough = false;
    bool        hasDoor = false;
    bool        reachable = false;
    bool        hasObjects = false;
    std::string problem; // the first thing wrong, for the player; empty if it works
};

const LairRoom*   FindLairRoom(const GameState& state, uint16_t id);
const LairRoom*   FindLairRoomAt(const GameState& state, const TileCoord& tile);
const LairObject* FindLairObjectAt(const GameState& state, const TileCoord& tile);
const LairDoor*   FindLairDoor(const GameState& state, const LairEdge& edge);
const LairJob*    FindLairDigJob(const GameState& state, const TileCoord& tile);
const LairJob*    FindLairJobFor(const GameState& state, LairJobKind kind, int target);
LairRoomStatus    GetLairRoomStatus(const GameState& state, const LairRoom& room);
bool              IsLairTileReachable(const GameState& state, const TileCoord& tile);
// Someone can walk up to this earth tile now. A marked tile that can't waits for its neighbors.
bool              IsLairDigReachable(const GameState& state, const TileCoord& tile);
// The footprint of an object, wherever it's placed.
std::vector<TileCoord> GetLairObjectTiles(const LairObjectType& type, const TileCoord& tile, int rotation);
// Middle of a footprint on the floor, and an edge's middle.
Vec3 GetLairObjectCenter(const GameState& state, const LairObjectType& type, const TileCoord& tile, int rotation);
Vec3 GetLairEdgeCenter(const GameState& state, const LairEdge& edge);
LairEdge MakeLairEdge(const TileCoord& tile, EdgeSide side);
// The two tiles an edge lies between.
void GetLairEdgeTiles(const LairEdge& edge, TileCoord& a, TileCoord& b);

// How many crew you can keep: kLairBaseCrewCapacity plus the beds in working rooms.
int CrewCapacity(const GameState& state);
// Crates that fit beyond the warehouse's own space, from shelves in working storage rooms.
int ExtraCrateCapacity(const GameState& state);
// Money a raid can't touch, from safes in working vaults (for later).
int64_t ProtectedMoney(const GameState& state);

// ---- Changing the lair (lair_build.cpp). Check* never changes anything; Place* checks first. ----

// A yes or no for the player, with the price.
struct LairCheck
{
    bool        ok = true;
    int         cost = 0;
    std::string reason; // why not
};

LairCheck CheckLairDig(const GameState& state, const TileCoord& tile);
bool      MarkLairDig(GameState& state, const TileCoord& tile);
bool      CancelLairDig(GameState& state, const TileCoord& tile);

LairCheck CheckLairRoom(const GameState& state, int type, const LairRect& rect);
LairCheck PlaceLairRoom(GameState& state, int type, const LairRect& rect);
LairCheck CheckLairDoor(const GameState& state, const LairEdge& edge);
LairCheck PlaceLairDoor(GameState& state, const LairEdge& edge);
LairCheck CheckLairObject(const GameState& state, int type, const TileCoord& tile, int rotation);
LairCheck PlaceLairObject(GameState& state, int type, const TileCoord& tile, int rotation);

// Deleting: built things refund kLairRefundFraction, things not yet built are cancelled and refund
// everything. `cost` of the check is the refund.
LairCheck CheckDeleteLairDoor(const GameState& state, const LairDoor& door);
LairCheck DeleteLairDoor(GameState& state, int doorId);
LairCheck CheckDeleteLairObject(const GameState& state, const LairObject& object);
LairCheck DeleteLairObject(GameState& state, int objectId);
LairCheck CheckDeleteLairRoom(const GameState& state, const LairRoom& room);
LairCheck DeleteLairRoom(GameState& state, uint16_t roomId);

// A raid took it: gone, with no refund.
void RemoveLairObjectForRaid(GameState& state, int objectId);

// Right click: cancels whatever job stands on this tile, if any, and refunds it.
bool CancelLairJobAt(GameState& state, const TileCoord& tile);

// Internal between lair_sim.cpp and lair_build.cpp.
void RefreshLairReachable(GameState& state);
// A room's new walls go up.
void RaiseLairRoomWalls(GameState& state, const LairRoom& room);
