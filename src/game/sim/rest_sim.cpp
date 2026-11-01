#include "rest_sim.h"

#include "game_state.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace
{
    constexpr WorldLevel kLevel       = WorldLevel::Underground;
    constexpr float      kBedSeconds  = 0.25f; // how often men off work look for a bed (sim seconds)

    constexpr struct { int dx, dz; EdgeSide side; } kSides[4] = {
        { 1, 0, EdgeSide::PosX }, { -1, 0, EdgeSide::NegX }, { 0, 1, EdgeSide::PosZ }, { 0, -1, EdgeSide::NegZ },
    };

    TileCoord TileUnder(const GameState& state, const CrewMember& member)
    {
        return state.world.WorldToTile(member.motion.level, member.motion.position);
    }

    bool IsBedWorking(const GameState& state, const LairObject& object)
    {
        if (!object.built || !GetLairObjectTypes()[(size_t)object.type].bed)
            return false;
        const LairRoom* room = FindLairRoom(state, object.room);
        return room && GetLairRoomStatus(state, *room).working;
    }

    const LairObject* FindBed(const GameState& state, int objectId)
    {
        for (const LairObject& object : state.lair.objects)
            if (object.id == objectId && IsBedWorking(state, object))
                return &object;
        return nullptr;
    }

    bool IsBedTaken(const GameState& state, int objectId)
    {
        for (const CrewMember& member : state.crew)
            if (member.restBed == objectId)
                return true;
        return false;
    }

    // A tile beside the bed to rest on, one nobody else has if it can be helped. There's no lying
    // down yet, so he stands by it; a bed is solid, and nobody stands in one.
    bool FindBedSpot(const GameState& state, const LairObject& bed, const CrewMember& member, TileCoord& spot)
    {
        const WorldGrid& world = state.world;
        bool found = false;
        for (const auto& s : kSides)
        {
            const TileCoord next = { bed.tile.level, bed.tile.x + s.dx, bed.tile.z + s.dz };
            if (!world.IsWalkable(next) || world.GetEdge(bed.tile, s.side) == EdgeKind::Wall || !IsLairTileReachable(state, next))
                continue;
            if (const LairObject* other = FindLairObjectAt(state, next); other && !other->built)
                continue; // about to have something on it
            bool shared = false;
            for (const CrewMember& other : state.crew)
                shared = shared || (other.id != member.id && other.restBed != 0 && other.restSpot == next);
            if (!shared)
            {
                spot = next;
                return true;
            }
            spot  = next;
            found = true;
        }
        return found;
    }

    // Somewhere out of the way to stand with no bed: right here if he's on the lair's plain floor,
    // otherwise a place below, where the beds will be.
    bool IsGoodToWait(const GameState& state, const TileCoord& tile)
    {
        if (tile.level != kLevel || !state.world.IsWalkable(tile) || state.world.GetTile(tile).kind != TileKind::Floor || !IsLairTileReachable(state, tile))
            return false;
        if (const LairRoom* room = FindLairRoomAt(state, tile); room && !room->built)
            return false;
        const LairObject* object = FindLairObjectAt(state, tile);
        return !object || object->built;
    }

    void SendToWait(GameState& state, CrewMember& member)
    {
        TileCoord goal = TileUnder(state, member);
        if (!IsGoodToWait(state, goal))
        {
            std::vector<TileCoord> tiles;
            for (int z = 0; z < state.world.GetDepth(); ++z)
                for (int x = 0; x < state.world.GetWidth(); ++x)
                    if (IsGoodToWait(state, { kLevel, x, z }))
                        tiles.push_back({ kLevel, x, z });
            if (tiles.empty())
                return; // nowhere: he stays where he is
            goal = tiles[state.random() % tiles.size()];
        }
        SendCharacterTo(member.motion, state.world, goal);
    }

    void LeaveBed(CrewMember& member)
    {
        member.restBed = 0;
        member.inBed   = false;
    }

    // Back to work. Whoever needs him next takes him.
    void BackOnDuty(CrewMember& member)
    {
        LeaveBed(member);
        member.jobId    = 0;
        member.idleTime = 0.5f;
    }

    void GoOffWork(GameState& state, CrewMember& member)
    {
        member.jobId        = kRestJobId;
        member.tiredSince   = state.clock.GetTotalMinutes();
        member.motion.speed = CharacterMotion{}.speed; // whatever he was carrying, he's set it down
        SendToWait(state, member);
    }

    void UpdateEnergy(GameState& state, CrewMember& member, float hours)
    {
        if (member.role == CrewRole::Boss || state.city.FindOut(member.id))
            return; // wounded or locked up is neither work nor rest: the meter waits
        float change = -kEnergyLossIdlePerHour;
        if (member.jobId == kRestJobId)
            change = member.inBed ? kEnergyGainInBedPerHour : kEnergyGainNoBedPerHour;
        else if (member.jobId != 0)
            change = -kEnergyLossWorkingPerHour; // anything anyone gave him, a mission's car included
        member.energy = std::clamp(member.energy + change * hours, 0.0f, kEnergyFull);
    }

    // Longest off work first: when beds are short, men take turns.
    void HandOutBeds(GameState& state)
    {
        std::vector<CrewMember*> waiting;
        for (CrewMember& member : state.crew)
            if (member.jobId == kRestJobId && member.restBed == 0)
                waiting.push_back(&member);
        if (waiting.empty())
            return;
        std::stable_sort(waiting.begin(), waiting.end(), [](const CrewMember* a, const CrewMember* b) { return a->tiredSince < b->tiredSince; });

        bool noBed = false;
        for (CrewMember* member : waiting)
        {
            // The nearest free one he can get to.
            std::vector<const LairObject*> beds;
            for (const LairObject& object : state.lair.objects)
                if (IsBedWorking(state, object) && !IsBedTaken(state, object.id))
                    beds.push_back(&object);
            const TileCoord here = TileUnder(state, *member);
            std::sort(beds.begin(), beds.end(), [&here](const LairObject* a, const LairObject* b) {
                return std::abs(a->tile.x - here.x) + std::abs(a->tile.z - here.z) < std::abs(b->tile.x - here.x) + std::abs(b->tile.z - here.z);
            });

            bool taken = false;
            for (const LairObject* bed : beds)
            {
                TileCoord spot;
                if (!FindBedSpot(state, *bed, *member, spot) || !SendCharacterTo(member->motion, state.world, spot))
                    continue;
                member->restBed  = bed->id;
                member->restSpot = spot;
                taken = true;
                break;
            }
            noBed = noBed || !taken;
        }

        const double now = state.clock.GetTotalMinutes();
        if (noBed && now - state.rest.lastNoBedNews >= (double)kNoBedNewsHours * 60.0)
        {
            state.rest.lastNoBedNews = now;
            state.PostEvent(GameEventKind::Bad, CountBeds(state) == 0 ? "The men are tired and there's nowhere to sleep. Build Bunks in the lair." : "Tired men are waiting for a bed. More bunks would help.");
        }
    }

    // On his way to a bed, or in it.
    void UpdateBed(GameState& state, CrewMember& member)
    {
        const LairObject* bed = FindBed(state, member.restBed);
        if (!bed || !state.world.IsWalkable(member.restSpot))
        {
            // It's gone, or its room stopped working: he goes back to looking.
            LeaveBed(member);
            return;
        }
        if (member.inBed || member.motion.IsMoving())
            return;
        if (TileUnder(state, member) != member.restSpot)
        {
            if (!SendCharacterTo(member.motion, state.world, member.restSpot))
                LeaveBed(member);
            return;
        }
        member.inBed = true;
        const Vec3 toBed = state.world.TileCenter(bed->tile) - member.motion.position;
        member.motion.yaw = std::atan2(-toBed.x, -toBed.z);
    }
}

void RestSim::Tick(GameState& state, const SimStep& step)
{
    if (step.simSeconds <= 0.0f)
        return;

    const bool  raid  = state.law.phase != RaidPhase::None;
    const float hours = step.gameMinutes / 60.0f;

    for (CrewMember& member : state.crew)
    {
        UpdateEnergy(state, member, hours);
        if (member.role == CrewRole::Boss)
            continue;

        // Someone took him off rest (a raid's aftermath, a dev command): the bed's free.
        if (member.jobId != kRestJobId)
            LeaveBed(member);

        if (member.jobId == kRestJobId)
        {
            // The raid comes first: up, and ready for whatever the family needs of him.
            if (raid)
                BackOnDuty(member);
            else if (member.energy >= (member.restBed != 0 ? kRestedEnergy : kFitEnergy))
                BackOnDuty(member);
            continue;
        }

        // A man still on his mission (walking out, in the car) finishes it. So does anyone in a raid.
        if (raid || member.away || member.jobId == CitySim::kMissionJobId || member.energy >= kTiredEnergy)
            continue;
        GoOffWork(state, member);
    }

    for (CrewMember& member : state.crew)
        if (member.jobId == kRestJobId && member.restBed != 0)
            UpdateBed(state, member);

    bedTimer -= step.simSeconds;
    if (bedTimer <= 0.0f)
    {
        bedTimer = kBedSeconds;
        HandOutBeds(state);
    }
}

bool IsFitForWork(const GameState& state, const CrewMember& member)
{
    if (member.jobId == kRestJobId)
        return false;
    return member.energy >= kTiredEnergy || state.law.phase != RaidPhase::None;
}

int CountBeds(const GameState& state)
{
    int count = 0;
    for (const LairObject& object : state.lair.objects)
        count += IsBedWorking(state, object) ? 1 : 0;
    return count;
}

int CountFreeBeds(const GameState& state)
{
    int count = 0;
    for (const LairObject& object : state.lair.objects)
        count += IsBedWorking(state, object) && !IsBedTaken(state, object.id) ? 1 : 0;
    return count;
}

int CountMenInBed(const GameState& state)
{
    int count = 0;
    for (const CrewMember& member : state.crew)
        count += member.inBed ? 1 : 0;
    return count;
}

int CountMenOffWork(const GameState& state)
{
    int count = 0;
    for (const CrewMember& member : state.crew)
        count += member.jobId == kRestJobId ? 1 : 0;
    return count;
}
