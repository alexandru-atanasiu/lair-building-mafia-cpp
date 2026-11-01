#include "lair_sim.h"

#include "game_state.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

// The job queue and what crew do. What may be placed: lair_build.cpp.

namespace
{
    constexpr WorldLevel kLevel = WorldLevel::Underground;

    // Tuning.
    constexpr float kDispatchSeconds    = 0.25f; // how often free crew look for work (sim seconds)
    constexpr float kMinIdleSeconds     = 1.5f;
    constexpr float kMaxIdleSeconds     = 5.0f;
    constexpr int   kUseObjectChance    = 70;    // percent of idle crew who head for a working room's object
    constexpr int   kOtherLevelChance   = 10;    // percent of wanders that cross to the warehouse and back
    constexpr float kTurnSpeed          = 8.0f;  // radians per sim second, turning to face work

    constexpr int kSteps[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

    constexpr int kFar = std::numeric_limits<int>::max();

    float WrapAngle(float a)
    {
        while (a > kPi)
            a -= 2.0f * kPi;
        while (a < -kPi)
            a += 2.0f * kPi;
        return a;
    }

    void TurnToward(CharacterMotion& motion, Vec3 point, float seconds)
    {
        const Vec3 d = point - motion.position;
        if (d.x * d.x + d.z * d.z < 1e-4f)
            return;
        const float wanted = std::atan2(-d.x, -d.z); // the model's forward is -Z
        const float turn   = WrapAngle(wanted - motion.yaw);
        const float step   = kTurnSpeed * seconds;
        motion.yaw = WrapAngle(motion.yaw + std::clamp(turn, -step, step));
    }

    LairCrewTask& GetTask(LairSim& lair, uint32_t crewId)
    {
        for (LairCrewTask& task : lair.tasks)
            if (task.crewId == crewId)
                return task;
        LairCrewTask task;
        task.crewId = crewId;
        lair.tasks.push_back(task);
        return lair.tasks.back();
    }

    TileCoord TileUnder(const GameState& state, const CrewMember& member)
    {
        return state.world.WorldToTile(member.motion.level, member.motion.position);
    }

    bool IsSpotClaimed(const LairSim& lair, const TileCoord& tile, uint32_t except)
    {
        for (const LairCrewTask& task : lair.tasks)
            if (task.crewId != except && task.hasSpot && task.spot == tile)
                return true;
        return false;
    }

    // A tile nobody should stand on just now: a room going up around it, an object about to.
    bool IsBuildSite(const GameState& state, const TileCoord& tile)
    {
        if (tile.level != kLevel)
            return false;
        if (const LairRoom* room = FindLairRoomAt(state, tile); room && !room->built)
            return true;
        const LairObject* object = FindLairObjectAt(state, tile);
        return object && !object->built;
    }

    // Tiles a worker can do this job from. Every one is walkable and an open step from the work.
    void GetWorkSpots(const GameState& state, const LairJob& job, std::vector<TileCoord>& spots)
    {
        spots.clear();
        const WorldGrid& world = state.world;

        const auto addAround = [&](const TileCoord& tile) {
            static constexpr EdgeSide kSides[4] = { EdgeSide::PosX, EdgeSide::NegX, EdgeSide::PosZ, EdgeSide::NegZ };
            for (int i = 0; i < 4; ++i)
            {
                const TileCoord next = { tile.level, tile.x + kSteps[i][0], tile.z + kSteps[i][1] };
                if (world.IsWalkable(next) && world.GetEdge(tile, kSides[i]) != EdgeKind::Wall && !IsBuildSite(state, next))
                    spots.push_back(next);
            }
        };

        switch (job.kind)
        {
        case LairJobKind::Dig:
            addAround(job.tile);
            break;
        case LairJobKind::Room:
        {
            const LairRoom* room = FindLairRoom(state, (uint16_t)job.target);
            if (!room)
                break;
            const LairRect& r = room->rect;
            for (int x = r.x0; x <= r.x1; ++x)
            {
                addAround({ kLevel, x, r.z0 });
                addAround({ kLevel, x, r.z1 });
            }
            for (int z = r.z0; z <= r.z1; ++z)
            {
                addAround({ kLevel, r.x0, z });
                addAround({ kLevel, r.x1, z });
            }
            break;
        }
        case LairJobKind::Door:
        {
            for (const LairDoor& door : state.lair.doors)
            {
                if (door.id != job.target)
                    continue;
                if (world.GetEdge(door.edge.tile, door.edge.side) != EdgeKind::Wall)
                    break; // waits for its wall
                TileCoord a, b;
                GetLairEdgeTiles(door.edge, a, b);
                for (const TileCoord& side : { a, b })
                    if (world.IsWalkable(side) && !IsBuildSite(state, side))
                        spots.push_back(side);
            }
            break;
        }
        case LairJobKind::Object:
        {
            for (const LairObject& object : state.lair.objects)
            {
                if (object.id != job.target)
                    continue;
                const LairRoom* room = FindLairRoom(state, object.room);
                if (!room || !room->built)
                    break; // waits for its room
                for (const TileCoord& tile : GetLairObjectTiles(GetLairObjectTypes()[(size_t)object.type], object.tile, object.rotation))
                    addAround(tile);
            }
            break;
        }
        }
    }

    // Where a worker looks while doing a job.
    Vec3 GetWorkPoint(const GameState& state, const LairJob& job, const TileCoord& spot)
    {
        switch (job.kind)
        {
        case LairJobKind::Dig:
            return state.world.TileCenter(job.tile);
        case LairJobKind::Room:
            if (const LairRoom* room = FindLairRoom(state, (uint16_t)job.target))
                return state.world.TileCenter({ kLevel, (room->rect.x0 + room->rect.x1) / 2, (room->rect.z0 + room->rect.z1) / 2 });
            break;
        case LairJobKind::Door:
            for (const LairDoor& door : state.lair.doors)
                if (door.id == job.target)
                    return GetLairEdgeCenter(state, door.edge);
            break;
        case LairJobKind::Object:
            for (const LairObject& object : state.lair.objects)
                if (object.id == job.target)
                    return GetLairObjectCenter(state, GetLairObjectTypes()[(size_t)object.type], object.tile, object.rotation);
            break;
        }
        return state.world.TileCenter(spot);
    }

    int CountWorkers(const GameState& state, int jobId)
    {
        int count = 0;
        for (const CrewMember& member : state.crew)
            if (member.jobId == jobId)
                ++count;
        return count;
    }

    // Steps from a crew member to every tile, 4-connected, across the stairs.
    void WalkDistances(const WorldGrid& world, const TileCoord& from, std::vector<int>& distances)
    {
        const int  width = world.GetWidth(), depth = world.GetDepth();
        const auto index = [&](const TileCoord& c) { return ((size_t)c.level * (size_t)depth + (size_t)c.z) * (size_t)width + (size_t)c.x; };

        distances.assign((size_t)kWorldLevelCount * (size_t)width * (size_t)depth, kFar);
        if (!world.InBounds(from))
            return;

        std::vector<TileCoord> frontier = { from }, next;
        distances[index(from)] = 0;
        int steps = 0;
        while (!frontier.empty())
        {
            ++steps;
            next.clear();
            for (const TileCoord& c : frontier)
            {
                const auto visit = [&](const TileCoord& t) {
                    if (distances[index(t)] == kFar)
                    {
                        distances[index(t)] = steps;
                        next.push_back(t);
                    }
                };
                for (const auto& step : kSteps)
                    if (world.CanStep(c, step[0], step[1]))
                        visit({ c.level, c.x + step[0], c.z + step[1] });
                for (const Stair& stair : world.GetStairs())
                {
                    if (stair.upper == c && world.IsWalkable(stair.lower))
                        visit(stair.lower);
                    else if (stair.lower == c && world.IsWalkable(stair.upper))
                        visit(stair.upper);
                }
            }
            frontier.swap(next);
        }
    }

    size_t DistanceIndex(const WorldGrid& world, const TileCoord& c)
    {
        return ((size_t)c.level * (size_t)world.GetDepth() + (size_t)c.z) * (size_t)world.GetWidth() + (size_t)c.x;
    }

    struct FreeWorker
    {
        CrewMember*      member;
        std::vector<int> distances;
    };

    // Who's free for work: the boss doesn't dig.
    bool IsFreeForWork(const CrewMember& member)
    {
        return !member.away && member.jobId == 0 && member.role != CrewRole::Boss;
    }

    // Who's free and fit for it.
    bool CanTakeWork(const GameState& state, const CrewMember& member)
    {
        return IsFreeForWork(member) && IsFitForWork(state, member);
    }

    bool CanStartJob(const GameState& state, const LairJob& job)
    {
        return CountWorkers(state, job.id) < job.maxWorkers;
    }

    // Gives `job` to the nearest of the free workers, with their spot. False if none can reach it.
    bool AssignNearest(GameState& state, const LairJob& job, std::vector<FreeWorker>& workers, const std::vector<TileCoord>& spots)
    {
        int        bestDistance = kFar;
        size_t     bestWorker   = 0;
        TileCoord  bestSpot;
        for (size_t w = 0; w < workers.size(); ++w)
        {
            for (const TileCoord& spot : spots)
            {
                if (IsSpotClaimed(state.lair, spot, workers[w].member->id))
                    continue;
                const int distance = workers[w].distances[DistanceIndex(state.world, spot)];
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    bestWorker   = w;
                    bestSpot     = spot;
                }
            }
        }
        if (bestDistance == kFar)
            return false;

        CrewMember& member = *workers[bestWorker].member;
        if (!SendCharacterTo(member.motion, state.world, bestSpot))
            return false;
        member.jobId = job.id;

        LairCrewTask& task = GetTask(state.lair, member.id);
        task.hasSpot = true;
        task.spot    = bestSpot;
        task.faces   = true;
        task.faceAt  = GetWorkPoint(state, job, bestSpot);

        workers.erase(workers.begin() + (std::ptrdiff_t)bestWorker);
        return true;
    }

    void DispatchJobs(GameState& state)
    {
        if (state.lair.jobs.empty())
            return;

        // The truck comes first: leave it the men it can use.
        int forTruck = CarriersWanted(state);

        std::vector<FreeWorker> workers;
        for (CrewMember& member : state.crew)
        {
            if (!CanTakeWork(state, member))
                continue;
            if (forTruck > 0)
            {
                --forTruck;
                continue;
            }
            FreeWorker worker;
            worker.member = &member;
            TileCoord from = TileUnder(state, member);
            if (!state.world.IsWalkable(from))
                FindNearestWalkable(state.world, from, 8, from);
            WalkDistances(state.world, from, worker.distances);
            workers.push_back(std::move(worker));
        }

        std::vector<TileCoord> spots;

        // Building comes first, oldest first: a door shouldn't wait behind a big dig.
        for (const LairJob& job : state.lair.jobs)
        {
            if (job.kind == LairJobKind::Dig)
                continue;
            while (!workers.empty() && CanStartJob(state, job))
            {
                GetWorkSpots(state, job, spots);
                if (!AssignNearest(state, job, workers, spots))
                    break;
            }
        }

        // Then digging: each free man takes the marked tile nearest to him.
        while (!workers.empty())
        {
            int        bestDistance = kFar;
            const LairJob* bestJob  = nullptr;
            size_t     bestWorker   = 0;
            for (const LairJob& job : state.lair.jobs)
            {
                if (job.kind != LairJobKind::Dig || !CanStartJob(state, job))
                    continue;
                GetWorkSpots(state, job, spots);
                for (size_t w = 0; w < workers.size(); ++w)
                {
                    for (const TileCoord& spot : spots)
                    {
                        const int distance = workers[w].distances[DistanceIndex(state.world, spot)];
                        if (distance < bestDistance && !IsSpotClaimed(state.lair, spot, workers[w].member->id))
                        {
                            bestDistance = distance;
                            bestJob      = &job;
                            bestWorker   = w;
                        }
                    }
                }
            }
            if (!bestJob)
                break;
            std::vector<FreeWorker> one;
            one.push_back(std::move(workers[bestWorker]));
            workers.erase(workers.begin() + (std::ptrdiff_t)bestWorker);
            GetWorkSpots(state, *bestJob, spots);
            AssignNearest(state, *bestJob, one, spots);
        }
    }

    void Release(GameState& state, CrewMember& member)
    {
        member.jobId    = 0;
        member.idleTime = 0.5f;
        LairCrewTask& task = GetTask(state.lair, member.id);
        task.hasSpot = false;
        task.faces   = false;
    }

    void FinishJob(GameState& state, const LairJob& job)
    {
        switch (job.kind)
        {
        case LairJobKind::Dig:
            state.world.SetTileKind(job.tile, TileKind::Floor);
            break;
        case LairJobKind::Room:
            for (LairRoom& room : state.lair.rooms)
            {
                if (room.id == job.target)
                {
                    room.built = true;
                    RaiseLairRoomWalls(state, room);
                    state.PostEvent(GameEventKind::Info, std::string(GetLairRoomTypes()[(size_t)room.type].name) + " walls are up");
                }
            }
            break;
        case LairJobKind::Door:
            for (LairDoor& door : state.lair.doors)
            {
                if (door.id == job.target)
                {
                    door.built = true;
                    state.world.SetEdge(door.edge.tile, door.edge.side, EdgeKind::Door);
                }
            }
            break;
        case LairJobKind::Object:
            for (LairObject& object : state.lair.objects)
            {
                if (object.id != job.target)
                    continue;
                object.built = true;
                const LairObjectType& type = GetLairObjectTypes()[(size_t)object.type];
                if (type.solid)
                    for (const TileCoord& tile : GetLairObjectTiles(type, object.tile, object.rotation))
                        state.world.SetTileSolid(tile, true);
            }
            break;
        }
    }

    // Anyone standing inside a room whose walls are about to close.
    bool IsSomeoneInside(const GameState& state, const LairRect& rect)
    {
        for (const CrewMember& member : state.crew)
        {
            if (member.away || member.motion.level != kLevel)
                continue;
            const TileCoord tile = TileUnder(state, member);
            if (rect.Contains(tile.x, tile.z))
                return true;
        }
        for (const LawAgent& agent : state.law.agents)
        {
            const TileCoord tile = state.world.WorldToTile(agent.motion.level, agent.motion.position);
            if (agent.motion.level == kLevel && rect.Contains(tile.x, tile.z))
                return true;
        }
        return false;
    }

    void UpdateWorkers(GameState& state, const SimStep& step)
    {
        // A job's workers add up.
        for (CrewMember& member : state.crew)
        {
            if (member.jobId < kLairFirstJobId)
                continue; // someone else's work, or none

            const auto job = std::find_if(state.lair.jobs.begin(), state.lair.jobs.end(), [&](const LairJob& j) { return j.id == member.jobId; });
            if (job == state.lair.jobs.end() || member.away)
            {
                Release(state, member);
                continue;
            }

            LairCrewTask& task = GetTask(state.lair, member.id);
            if (member.motion.IsMoving())
                continue;
            if (!task.hasSpot || TileUnder(state, member) != task.spot)
            {
                // Lost the way, or the spot went: back to the queue.
                if (!task.hasSpot || !SendCharacterTo(member.motion, state.world, task.spot))
                    Release(state, member);
                continue;
            }

            TurnToward(member.motion, task.faceAt, step.simSeconds);
            job->progress += step.simSeconds;
        }

        // Done jobs. Finishing changes the world; workers re-plan by themselves.
        for (size_t i = 0; i < state.lair.jobs.size();)
        {
            const LairJob job = state.lair.jobs[i];
            bool done = job.progress >= job.duration;
            if (done && job.kind == LairJobKind::Room)
            {
                // The walls wait until the room is clear: nobody gets walled in.
                const LairRoom* room = FindLairRoom(state, (uint16_t)job.target);
                done = room && !IsSomeoneInside(state, room->rect);
                state.lair.jobs[i].progress = job.duration;
            }
            if (!done)
            {
                ++i;
                continue;
            }

            FinishJob(state, job);
            state.lair.jobs.erase(state.lair.jobs.begin() + (std::ptrdiff_t)i);
            for (CrewMember& member : state.crew)
                if (member.jobId == job.id)
                    Release(state, member);
        }
    }

    // Sends idle crew out of the way: off anything solid, out of rooms going up.
    void ClearBuildSites(GameState& state)
    {
        for (CrewMember& member : state.crew)
        {
            if (member.away || member.motion.level != kLevel || member.motion.IsMoving())
                continue;
            const TileCoord tile = TileUnder(state, member);

            if (!state.world.IsWalkable(tile))
            {
                TileCoord free;
                if (FindNearestWalkable(state.world, tile, 8, free))
                    SendCharacterTo(member.motion, state.world, free);
                continue;
            }
            if (member.jobId != 0 && member.jobId != kRestJobId)
                continue; // a tired man standing about moves aside too

            const LairRoom* room = FindLairRoomAt(state, tile);
            if (!room || room->built)
                continue;

            // The nearest tile that isn't part of it.
            TileCoord best;
            int       bestDistance = kFar;
            for (int z = room->rect.z0 - 1; z <= room->rect.z1 + 1; ++z)
            {
                for (int x = room->rect.x0 - 1; x <= room->rect.x1 + 1; ++x)
                {
                    const TileCoord c = { kLevel, x, z };
                    if (room->rect.Contains(x, z) || !state.world.IsWalkable(c) || IsBuildSite(state, c))
                        continue;
                    const int distance = std::abs(x - tile.x) + std::abs(z - tile.z);
                    if (distance < bestDistance)
                    {
                        bestDistance = distance;
                        best         = c;
                    }
                }
            }
            if (bestDistance != kFar)
                SendCharacterTo(member.motion, state.world, best);
        }
    }

    // A free spot to use `object` from.
    bool FindUseSpot(const GameState& state, const LairObject& object, const CrewMember& member, TileCoord& spot)
    {
        const LairObjectType& type = GetLairObjectTypes()[(size_t)object.type];
        if (type.seat)
        {
            if (IsSpotClaimed(state.lair, object.tile, member.id))
                return false;
            spot = object.tile;
            return true;
        }

        std::vector<TileCoord> around;
        LairJob                probe;
        probe.kind   = LairJobKind::Object;
        probe.target = object.id;
        GetWorkSpots(state, probe, around);
        for (size_t i = 0; i < around.size(); ++i)
        {
            const TileCoord& c = around[((size_t)member.id + i) % around.size()]; // crew spread around it
            // Not on another object's seat, and not walled away.
            if (!IsSpotClaimed(state.lair, c, member.id) && !FindLairObjectAt(state, c) && IsLairTileReachable(state, c))
            {
                spot = c;
                return true;
            }
        }
        return false;
    }

    // What a user of a seat faces: the nearest solid thing in the room, or the way the seat faces.
    Vec3 GetSeatFacing(const GameState& state, const LairObject& seat)
    {
        const Vec3 at = state.world.TileCenter(seat.tile);
        Vec3       best = at + Vec3{ std::sin((float)seat.rotation * 0.5f * kPi), 0.0f, std::cos((float)seat.rotation * 0.5f * kPi) };
        float      bestDistance = 1e30f;
        for (const LairObject& other : state.lair.objects)
        {
            const LairObjectType& type = GetLairObjectTypes()[(size_t)other.type];
            if (other.room != seat.room || !type.solid || !other.built)
                continue;
            const Vec3  center = GetLairObjectCenter(state, type, other.tile, other.rotation);
            const float d      = Length(center - at);
            if (d < bestDistance)
            {
                bestDistance = d;
                best         = center;
            }
        }
        return best;
    }

    bool SendToObject(GameState& state, CrewMember& member, const LairObject& object)
    {
        const LairObjectType& type = GetLairObjectTypes()[(size_t)object.type];
        TileCoord             spot;
        if (!FindUseSpot(state, object, member, spot) || !SendCharacterTo(member.motion, state.world, spot))
            return false;
        LairCrewTask& task = GetTask(state.lair, member.id);
        task.hasSpot = true;
        task.spot    = spot;
        task.faces   = true;
        task.faceAt  = type.seat ? GetSeatFacing(state, object) : GetLairObjectCenter(state, type, object.tile, object.rotation);
        member.idleTime = type.useSeconds + (float)(state.random() % 400) * 0.01f;
        return true;
    }

    // Something to go and use in a working room, at random.
    const LairObject* PickObjectToUse(GameState& state, const CrewMember& member)
    {
        std::vector<const LairObject*> candidates;
        for (const LairObject& object : state.lair.objects)
        {
            const LairObjectType& type = GetLairObjectTypes()[(size_t)object.type];
            if (!object.built || type.useSeconds <= 0.0f || type.bossSeat)
                continue;
            const LairRoom* room = FindLairRoom(state, object.room);
            TileCoord       spot;
            if (room && GetLairRoomStatus(state, *room).working && FindUseSpot(state, object, member, spot))
                candidates.push_back(&object);
        }
        return candidates.empty() ? nullptr : candidates[state.random() % candidates.size()];
    }

    bool PickFloorTile(GameState& state, WorldLevel level, TileCoord& tile)
    {
        std::vector<TileCoord> floors;
        for (int z = 0; z < state.world.GetDepth(); ++z)
        {
            for (int x = 0; x < state.world.GetWidth(); ++x)
            {
                const TileCoord c = { level, x, z };
                if (state.world.GetTile(c).kind == TileKind::Floor && state.world.IsWalkable(c) && IsLairTileReachable(state, c) &&
                    !IsBuildSite(state, c) && !IsSpotClaimed(state.lair, c, 0))
                    floors.push_back(c);
            }
        }
        if (floors.empty())
            return false;
        tile = floors[state.random() % floors.size()];
        return true;
    }

    // The boss's seat in a working office, if any.
    const LairObject* FindBossSeat(const GameState& state)
    {
        for (const LairObject& object : state.lair.objects)
        {
            if (!object.built || !GetLairObjectTypes()[(size_t)object.type].bossSeat)
                continue;
            const LairRoom* room = FindLairRoom(state, object.room);
            if (room && GetLairRoomStatus(state, *room).working)
                return &object;
        }
        return nullptr;
    }

    void UpdateIdleCrew(GameState& state, const SimStep& step)
    {
        const LairObject* bossSeat = FindBossSeat(state);

        for (CrewMember& member : state.crew)
        {
            if (member.away || member.jobId != 0)
                continue;

            LairCrewTask& task = GetTask(state.lair, member.id);
            if (member.motion.IsMoving())
                continue;

            if (task.faces)
                TurnToward(member.motion, task.faceAt, step.simSeconds);
            if (!state.lair.wanderEnabled)
                continue;

            // The boss stays at his desk, once he has one.
            if (member.role == CrewRole::Boss && bossSeat)
            {
                if (TileUnder(state, member) != bossSeat->tile)
                {
                    member.idleTime -= step.simSeconds;
                    if (member.idleTime <= 0.0f)
                    {
                        member.idleTime = 1.0f;
                        task.hasSpot    = false;
                        SendToObject(state, member, *bossSeat);
                    }
                }
                else
                {
                    task.hasSpot = true;
                    task.spot    = bossSeat->tile;
                    task.faces   = true;
                    task.faceAt  = GetSeatFacing(state, *bossSeat);
                }
                continue;
            }

            member.idleTime -= step.simSeconds;
            if (member.idleTime > 0.0f)
                continue;

            // On to something else.
            task.hasSpot = false;
            task.faces   = false;
            const float t = (float)(state.random() % 1000) / 1000.0f;
            member.idleTime = kMinIdleSeconds + t * (kMaxIdleSeconds - kMinIdleSeconds);

            if (member.role != CrewRole::Boss && (int)(state.random() % 100) < kUseObjectChance)
            {
                if (const LairObject* object = PickObjectToUse(state, member))
                    if (SendToObject(state, member, *object))
                        continue;
            }

            WorldLevel level = member.motion.level;
            if (member.role == CrewRole::Boss)
                level = kLevel; // the boss stays below
            else if ((int)(state.random() % 100) < kOtherLevelChance)
                level = level == WorldLevel::Ground ? kLevel : WorldLevel::Ground;

            TileCoord goal;
            if (PickFloorTile(state, level, goal) && SendCharacterTo(member.motion, state.world, goal))
            {
                task.hasSpot = true;
                task.spot    = goal;
            }
        }
    }
}

void LairSim::Tick(GameState& state, const SimStep& step)
{
    // Someone taken for other work, off to rest, or gone: what he meant to do here is off, and
    // leaves his spot for others. A job he left stays in the queue with its progress.
    for (const CrewMember& member : state.crew)
    {
        if (member.away || (member.jobId != 0 && member.jobId < kLairFirstJobId))
        {
            LairCrewTask& task = GetTask(state.lair, member.id);
            task.hasSpot = false;
            task.faces   = false;
        }
    }

    RefreshLairReachable(state);
    UpdateWorkers(state, step);
    RefreshLairReachable(state);

    dispatchTimer -= step.simSeconds;
    if (dispatchTimer <= 0.0f)
    {
        dispatchTimer = kDispatchSeconds;
        DispatchJobs(state);
    }

    ClearBuildSites(state);
    UpdateIdleCrew(state, step);
}
