#include "law_sim.h"

#include "game_state.h"
#include "../world/world_layout.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <functional>
#include <iterator>

// Tuning is at the top of law_sim.h.

namespace
{
    static_assert(kLawJobId < kLairFirstJobId && kLawJobId != CitySim::kMissionJobId);

    const Color kInvestigatorTint = { 0.80f, 0.64f, 0.36f, 1.0f }; // a trench coat
    const Color kOfficerTint      = { 0.08f, 0.20f, 0.72f, 1.0f }; // police blue

    const char* const kInvestigatorNames[] = { "Malone", "Novak", "Brennan", "Kelly", "Doyle", "Ferris", "Hart", "Quinn" };

    // The way in: from the road, along the front of the building, in by the front door at (19, 10).
    constexpr TileCoord kRoadStart    = { WorldLevel::Ground, 37, 3 };
    constexpr TileCoord kRoadTurn     = { WorldLevel::Ground, 36, 8 };
    constexpr TileCoord kFrontOutside = { WorldLevel::Ground, 19, 9 };
    constexpr TileCoord kFrontInside  = { WorldLevel::Ground, 19, 11 };

    // What he walks past upstairs: the racks' aisle and the overflow pallets.
    constexpr TileCoord kRackAisle     = { WorldLevel::Ground, 19, 21 };
    constexpr TileCoord kOverflowAisle = { WorldLevel::Ground, 24, 13 };
    // Where the officers who stay upstairs end up.
    constexpr TileCoord kOfficerSpots[] = {
        { WorldLevel::Ground, 24, 14 }, { WorldLevel::Ground, 21, 20 }, { WorldLevel::Ground, 16, 14 },
    };

    // The back way out, in a corner of the first basement.
    constexpr TileCoord kBoltHole = { WorldLevel::Underground, kStairFoot.x - 2, kStairFoot.z + 4 };

    float RandomUnit(GameState& state)
    {
        return (float)(state.random() % 1000) / 1000.0f;
    }

    TileCoord TileOf(const GameState& state, const CharacterMotion& motion)
    {
        return state.world.WorldToTile(motion.level, motion.position);
    }

    bool IsIndoors(const GameState& state, const CharacterMotion& motion)
    {
        return motion.level == WorldLevel::Underground || state.world.GetTile(TileOf(state, motion)).kind != TileKind::Pavement;
    }

    float FlatDistance(Vec3 a, Vec3 b)
    {
        const float dx = a.x - b.x;
        const float dz = a.z - b.z;
        return std::sqrt(dx * dx + dz * dz);
    }

    // A tile someone can walk to in this room, as near its middle as there is.
    bool FindTileInRoom(const GameState& state, const LairRoom& room, TileCoord& tile)
    {
        const int centerX = (room.rect.x0 + room.rect.x1) / 2;
        const int centerZ = (room.rect.z0 + room.rect.z1) / 2;
        constexpr int kNone = 1 << 20;
        int best = kNone;
        for (int z = room.rect.z0; z <= room.rect.z1; ++z)
        {
            for (int x = room.rect.x0; x <= room.rect.x1; ++x)
            {
                const TileCoord c = { WorldLevel::Underground, x, z };
                const int       distance = std::abs(x - centerX) + std::abs(z - centerZ);
                if (distance < best && state.world.IsWalkable(c) && IsLairTileReachable(state, c))
                {
                    best = distance;
                    tile = c;
                }
            }
        }
        return best != kNone;
    }

    // Places worth a look below: rooms first, in no particular order, then any bare floor.
    std::vector<TileCoord> PickLairTiles(GameState& state, int count)
    {
        std::vector<TileCoord>       tiles;
        std::vector<const LairRoom*> rooms;
        for (const LairRoom& room : state.lair.rooms)
            if (room.built)
                rooms.push_back(&room);
        while (!rooms.empty() && (int)tiles.size() < count)
        {
            const size_t pick = state.random() % rooms.size();
            TileCoord    tile;
            if (FindTileInRoom(state, *rooms[pick], tile))
                tiles.push_back(tile);
            rooms.erase(rooms.begin() + (std::ptrdiff_t)pick);
        }

        std::vector<TileCoord> floors;
        for (int z = 0; z < state.world.GetDepth(); ++z)
        {
            for (int x = 0; x < state.world.GetWidth(); ++x)
            {
                const TileCoord c = { WorldLevel::Underground, x, z };
                if (state.world.GetTile(c).kind == TileKind::Floor && state.world.IsWalkable(c) && IsLairTileReachable(state, c) && !(c == kStairFoot))
                    floors.push_back(c);
            }
        }
        while (!floors.empty() && (int)tiles.size() < count)
        {
            const size_t pick = state.random() % floors.size();
            tiles.push_back(floors[pick]);
            floors.erase(floors.begin() + (std::ptrdiff_t)pick);
        }
        return tiles;
    }

    // "Sal", "Sal and Tony", "Sal, Tony and Vinnie"
    std::string JoinNames(const std::vector<std::string>& names)
    {
        std::string text;
        for (size_t i = 0; i < names.size(); ++i)
        {
            text += names[i];
            if (i + 2 < names.size())
                text += ", ";
            else if (i + 2 == names.size())
                text += " and ";
        }
        return text;
    }

    // ---- What an investigator sees

    void LookAround(GameState& state, LawAgent& agent)
    {
        const Vec3      at   = agent.motion.position;
        const TileCoord here = TileOf(state, agent.motion);

        if (agent.motion.level == WorldLevel::Ground)
        {
            for (const StoredCrate& crate : state.warehouse.stored)
            {
                if (FlatDistance(crate.position, at) > kSightRadius)
                    continue;
                const auto known = std::find_if(agent.seenCrates.begin(), agent.seenCrates.end(), [&](const auto& seen) { return seen.first == crate.slot; });
                if (known == agent.seenCrates.end())
                    agent.seenCrates.push_back({ crate.slot, crate.kind });
            }
        }
        else
        {
            agent.wentBelow      = true;
            agent.looseMoneySeen = std::max(agent.looseMoneySeen, state.money - ProtectedMoney(state));

            // He sees into the room he's standing in. A shut room stays shut.
            if (const LairRoom* room = FindLairRoomAt(state, here))
            {
                for (const LairObject& object : state.lair.objects)
                {
                    if (object.room != room->id || !object.built || GetLairObjectTypes()[(size_t)object.type].evidence <= 0.0f)
                        continue;
                    if (std::find(agent.seenObjects.begin(), agent.seenObjects.end(), object.id) == agent.seenObjects.end())
                        agent.seenObjects.push_back(object.id);
                }
            }
        }

        for (const CrewMember& member : state.crew)
        {
            if (member.away || member.role == CrewRole::Boss || member.jobId != 0 || member.motion.level != agent.motion.level)
                continue;
            if (FlatDistance(member.motion.position, at) > kSightRadius || !HasClearLine(state.world, agent.motion.level, at, member.motion.position, 0.0f))
                continue;
            const LairRoom* room = FindLairRoomAt(state, TileOf(state, member.motion));
            if (room && GetLairRoomStatus(state, *room).working)
                continue; // off duty, in a proper room
            if (std::find(agent.seenMen.begin(), agent.seenMen.end(), member.id) == agent.seenMen.end())
                agent.seenMen.push_back(member.id);
        }
    }

    // He's left: what he saw becomes evidence, and a line in the news.
    void FileReport(GameState& state, const LawAgent& agent)
    {
        LawSim& law = state.law;
        ++law.reportsFiled;

        // Worst first onto the shelves: that's where a sensible man would put them.
        std::vector<int> kinds;
        for (const auto& seen : agent.seenCrates)
            kinds.push_back(seen.second);
        std::sort(kinds.begin(), kinds.end(), std::greater<int>());
        const int shelved = std::min(ExtraCrateCapacity(state), (int)kinds.size());

        float found = 0.0f;
        int   counts[std::size(kEvidencePerCrate)] = {};
        for (size_t i = 0; i < kinds.size(); ++i)
        {
            const int kind = std::clamp(kinds[i], 0, (int)std::size(kEvidencePerCrate) - 1);
            found += kEvidencePerCrate[kind] * ((int)i < shelved ? kShelvedCrateFactor : 1.0f);
            ++counts[kind];
        }

        std::vector<std::string> parts;
        for (int kind = (int)std::size(counts) - 1; kind >= 0; --kind)
            if (counts[kind] > 0)
                parts.push_back(std::to_string(counts[kind]) + " " + (counts[kind] == 1 ? "crate" : "crates") + " of " + GetShipmentKind(kind).lowerName);
        if (shelved > 0)
            parts.push_back(std::to_string(shelved) + " of them on shelves");

        std::vector<std::string> objects;
        for (const int id : agent.seenObjects)
        {
            for (const LairObject& object : state.lair.objects)
            {
                if (object.id != id)
                    continue;
                const LairObjectType& type = GetLairObjectTypes()[(size_t)object.type];
                found += type.evidence;
                objects.push_back(type.name);
            }
        }
        if (!objects.empty())
        {
            std::string first = objects[0];
            std::transform(first.begin(), first.end(), first.begin(), [](unsigned char c) { return (char)std::tolower(c); });
            parts.push_back(objects.size() == 1 ? "a " + first : std::to_string(objects.size()) + " guns and benches");
        }

        if (agent.wentBelow && agent.looseMoneySeen >= 1000)
        {
            found += std::min((float)agent.looseMoneySeen / 1000.0f * kEvidencePerLooseGrand, kLooseMoneyEvidenceMax);
            parts.push_back(WarehouseMoneyText(agent.looseMoneySeen) + " outside a vault");
        }
        if (!agent.seenMen.empty())
        {
            found += (float)agent.seenMen.size() * kIdleManEvidence;
            parts.push_back(std::to_string(agent.seenMen.size()) + (agent.seenMen.size() == 1 ? " man" : " men") + " loafing");
        }

        char ending[48];
        if (found < kCleanReportBelow)
        {
            law.AddEvidence(kCleanReportEvidence);
            snprintf(ending, sizeof(ending), "Evidence %d.", (int)kCleanReportEvidence);
            state.PostEvent(GameEventKind::Good, "Inspector " + agent.name + " found nothing to speak of. " + ending);
        }
        else
        {
            law.AddEvidence(found);
            snprintf(ending, sizeof(ending), "Evidence +%d.", (int)std::lround(found));
            std::string text = "Inspector " + agent.name + " reports ";
            for (size_t i = 0; i < parts.size(); ++i)
                text += (i > 0 ? ", " : "") + parts[i];
            state.PostEvent(GameEventKind::Bad, text + ". " + ending);
        }
        law.devLog.push_back("report day " + std::to_string(state.clock.GetDay()) + " " + std::to_string(state.clock.GetHour()) + "h found=" + std::to_string((int)found) +
                             " evidence=" + std::to_string((int)law.evidence) + " heat=" + std::to_string((int)state.heat));
    }

    LawAgent MakeAgent(GameState& state, LawAgentKind kind, const std::string& name, const Color& tint, const TileCoord& start)
    {
        LawAgent agent;
        agent.id   = state.nextCharacterId++;
        agent.kind = kind;
        agent.name = name;
        agent.tint = tint;
        PlaceCharacter(agent.motion, state.world, start);
        return agent;
    }
}

void LawSim::AddEvidence(float amount)
{
    evidence = std::clamp(evidence + amount, 0.0f, kEvidenceMax);
}

int LawSim::CountAgents(LawAgentKind kind) const
{
    return (int)std::count_if(agents.begin(), agents.end(), [kind](const LawAgent& agent) { return agent.kind == kind; });
}

void LawSim::SpawnInvestigators(GameState& state, int count)
{
    for (int i = 0; i < count; ++i)
    {
        const std::string name = kInvestigatorNames[(size_t)m_nextName++ % std::size(kInvestigatorNames)];
        // Spaced out along the road, so a party doesn't walk as one body.
        LawAgent agent = MakeAgent(state, LawAgentKind::Investigator, name, kInvestigatorTint, { WorldLevel::Ground, kRoadStart.x, kRoadStart.z - i });

        agent.route = { { kRoadTurn }, { kFrontOutside }, { kFrontInside, kLookSeconds * 0.5f }, { kRackAisle, kLookSeconds }, { kOverflowAisle, kLookSeconds } };
        if (IsLairTileReachable(state, kStairFoot))
        {
            agent.route.push_back({ kStairFoot, kLookSeconds * 0.5f });
            for (const TileCoord& tile : PickLairTiles(state, kLairStopsPerVisit))
                agent.route.push_back({ tile, kLookSeconds });
            agent.route.push_back({ kStairTop });
        }
        agent.route.push_back({ kFrontInside });
        agent.route.push_back({ kFrontOutside });
        agent.route.push_back({ kRoadTurn });
        agent.route.push_back({ kRoadStart });
        agents.push_back(std::move(agent));
        ++investigatorsSeen;

        state.PostEvent(GameEventKind::Bad, "An investigator is on his way: Inspector " + name + ". He'll report whatever he sees.");
    }
}

void LawSim::StartRaid(GameState& state, bool noWarning)
{
    if (phase != RaidPhase::None)
        return;
    devLog.push_back("raid warning day " + std::to_string(state.clock.GetDay()) + " " + std::to_string(state.clock.GetHour()) + "h heat=" +
                     std::to_string((int)state.heat) + " evidence=" + std::to_string((int)evidence) + " money=" + std::to_string(state.money));
    if (noWarning)
    {
        SendInOfficers(state);
        return;
    }
    phase           = RaidPhase::Warning;
    warningMinutes  = kRaidWarningHours * 60.0f;
    speedBeforeRaid = state.clock.GetSpeed();
    state.clock.SetSpeed(1);
    char text[160];
    snprintf(text, sizeof(text), "Word from the street: a raid in %d hours! Ship crates out. Men sent on a mission won't be caught.", (int)kRaidWarningHours);
    state.PostEvent(GameEventKind::Bad, text);
}

void LawSim::SendInOfficers(GameState& state)
{
    phase        = RaidPhase::Police;
    raidSeconds  = 0.0f;
    endSeconds   = 0.0f;
    policeInside = false;
    bossRunning  = false;

    // On the last raid the boss has nowhere to go: he stops where he is and they come for him.
    TileCoord bossTile = kStairFoot;
    if (IsFinalRaid())
    {
        for (CrewMember& member : state.crew)
        {
            if (member.role != CrewRole::Boss)
                continue;
            member.jobId = kLawJobId;
            TileCoord here = TileOf(state, member.motion);
            if (!state.world.IsWalkable(here))
                FindNearestWalkable(state.world, here, 8, here);
            PlaceCharacter(member.motion, state.world, here);
            bossTile = here;
        }
    }

    const std::vector<TileCoord> below = PickLairTiles(state, kRaidOfficers);
    size_t belowIndex = 0;
    for (int i = 0; i < kRaidOfficers; ++i)
    {
        LawAgent officer = MakeAgent(state, LawAgentKind::Officer, "Officer", kOfficerTint, { WorldLevel::Ground, kRoadStart.x - (i % 2), kRoadStart.z - i / 2 });
        officer.motion.speed = kOfficerSpeed;
        officer.route = { { kRoadTurn }, { kFrontOutside }, { kFrontInside } };
        if (i < (int)std::size(kOfficerSpots))
        {
            officer.route.push_back({ kOfficerSpots[i], 1.0f });
        }
        else
        {
            officer.route.push_back({ kStairFoot });
            if (IsFinalRaid() && i == (int)std::size(kOfficerSpots))
                officer.route.push_back({ bossTile });
            else if (belowIndex < below.size())
                officer.route.push_back({ below[belowIndex++], 1.0f });
        }
        agents.push_back(std::move(officer));
    }
    state.PostEvent(GameEventKind::Bad, IsFinalRaid() ? "The police are here, and this time they know about the bolt-hole." : "The police are here!");
}

void LawSim::BossReacts(GameState& state)
{
    if (IsFinalRaid())
    {
        state.PostEvent(GameEventKind::Bad, "The police are inside. There's no way out this time.");
        return;
    }
    for (CrewMember& member : state.crew)
    {
        if (member.role != CrewRole::Boss || member.away)
            continue;
        member.jobId = kLawJobId;
        member.motion.speed = kBossFleeSpeed;
        if (SendCharacterTo(member.motion, state.world, kBoltHole))
            bossRunning = true;
        else
            member.away = true; // the way's been built over: a family keeps more than one way out
        state.PostEvent(GameEventKind::Bad, "The police are inside! " + member.name + " runs for the bolt-hole.");
    }
}

void LawSim::TickSlowChanges(GameState& state, const SimStep& step)
{
    const float hours = step.gameMinutes / 60.0f;

    if (state.heat < kEvidenceDecayHeat)
        AddEvidence(-kEvidenceDecayPerHour * hours);

    // Goods that are known about: they raise heat just by being kept, unless they're on shelves.
    int counts[std::size(kStoredHeatPerHour)] = {};
    for (const StoredCrate& crate : state.warehouse.stored)
        ++counts[std::clamp(crate.kind, 0, (int)std::size(counts) - 1)];
    int   shelved = ExtraCrateCapacity(state);
    float perHour = 0.0f;
    for (int kind = (int)std::size(counts) - 1; kind >= 0; --kind)
    {
        const int hidden = std::min(shelved, counts[kind]);
        shelved -= hidden;
        perHour += (float)(counts[kind] - hidden) * kStoredHeatPerHour[kind];
    }
    if (perHour > 0.0f)
        state.AddHeat(perHour * hours);
}

void LawSim::TickVisits(GameState& state, const SimStep& step)
{
    if (phase != RaidPhase::None)
        return;

    const int band = GetHeatBand(state.heat);
    if (band == 0)
    {
        m_visitMinutes = -1.0f;
        m_visitBand    = 0;
        return;
    }

    const float interval = kVisitEveryHours[band] * 60.0f;
    if (m_visitMinutes < 0.0f || band > m_visitBand)
    {
        const float soon = interval * kFirstVisitFraction;
        m_visitMinutes   = m_visitMinutes < 0.0f ? soon : std::min(m_visitMinutes, soon);
    }
    m_visitBand = band;

    m_visitMinutes -= step.gameMinutes;
    if (m_visitMinutes > 0.0f)
        return;
    m_visitMinutes = interval * (1.0f - kVisitJitter + 2.0f * kVisitJitter * RandomUnit(state));

    const int room = kVisitorsAtOnce[band] - CountAgents(LawAgentKind::Investigator);
    if (room > 0)
        SpawnInvestigators(state, room);
}

void LawSim::TickAgents(GameState& state, const SimStep& step)
{
    for (LawAgent& agent : agents)
    {
        UpdateCharacterMotion(agent.motion, state.world, step.simSeconds);
        agent.totalSeconds += step.simSeconds;

        if (agent.kind == LawAgentKind::Investigator && IsIndoors(state, agent.motion))
        {
            agent.sightTimer -= step.simSeconds;
            if (agent.sightTimer <= 0.0f)
            {
                agent.sightTimer = 0.25f;
                LookAround(state, agent);
            }
        }

        if (agent.finished || agent.motion.IsMoving())
            continue;

        if (agent.pendingLook > 0.0f)
        {
            agent.lookSeconds = agent.pendingLook;
            agent.pendingLook = 0.0f;
        }
        if (agent.lookSeconds > 0.0f)
        {
            agent.lookSeconds -= step.simSeconds;
            agent.motion.yaw += step.simSeconds * 1.2f; // looking around
            continue;
        }

        // On to the next place. One he can't get to any more is skipped.
        bool sent = false;
        while (!sent && agent.nextStop < agent.route.size())
        {
            const LawStop& stop = agent.route[agent.nextStop++];
            if (SendCharacterTo(agent.motion, state.world, stop.tile))
            {
                agent.pendingLook = stop.lookSeconds;
                sent = true;
            }
        }
        if (!sent)
            agent.finished = true;
    }

    // Investigators leave once done, or if something held them up for too long.
    for (size_t i = 0; i < agents.size();)
    {
        const LawAgent& agent = agents[i];
        if (agent.kind == LawAgentKind::Investigator && (agent.finished || agent.totalSeconds > kMaxVisitSeconds))
        {
            FileReport(state, agent);
            departedIds.push_back(agent.id);
            agents.erase(agents.begin() + (std::ptrdiff_t)i);
            continue;
        }
        ++i;
    }
}

void LawSim::TickRaid(GameState& state, const SimStep& step)
{
    switch (phase)
    {
    case RaidPhase::None:
        if (state.heat >= kRaidHeat && evidence >= kRaidEvidence)
            StartRaid(state, false);
        break;

    case RaidPhase::Warning:
        warningMinutes -= step.gameMinutes;
        if (warningMinutes <= 0.0f)
            SendInOfficers(state);
        break;

    case RaidPhase::Police:
    {
        raidSeconds += step.simSeconds;

        bool allInPlace = true;
        for (const LawAgent& agent : agents)
        {
            if (agent.kind != LawAgentKind::Officer)
                continue;
            allInPlace = allInPlace && agent.finished;
            if (!policeInside && IsIndoors(state, agent.motion))
            {
                policeInside = true;
                BossReacts(state);
            }
        }

        if (bossRunning)
        {
            for (CrewMember& member : state.crew)
            {
                if (member.role != CrewRole::Boss || member.away || member.motion.IsMoving())
                    continue;
                member.away = true; // down the hole
                bossRunning = false;
                state.PostEvent(GameEventKind::Good, member.name + " is out through the bolt-hole.");
            }
        }

        if (allInPlace)
            endSeconds += step.simSeconds;
        if (endSeconds >= kRaidEndSeconds || raidSeconds >= kRaidMaxSeconds)
        {
            if (IsFinalRaid())
                BustBoss(state);
            else
                MoveHouse(state);
        }
        break;
    }
    }
}

void LawSim::ClearAgents()
{
    for (const LawAgent& agent : agents)
        departedIds.push_back(agent.id);
    agents.clear();
    phase        = RaidPhase::None;
    policeInside = false;
    bossRunning  = false;
}

void LawSim::MoveHouse(GameState& state)
{
    ++raidsSurvived;
    report = {};
    report.title = raidsSurvived == 1 ? "You got away" : "You got away again";

    // The money. Worked out before any safes go.
    const int64_t before    = std::max<int64_t>(state.money, 0);
    const int64_t safe      = std::min(ProtectedMoney(state), before);
    const int64_t moneyLost = (int64_t)std::llround((float)(before - safe) * kRaidLooseMoneyLoss + (float)safe * kRaidVaultMoneyLoss);
    state.money -= moneyLost;

    // The crates, and any truck: crates on it go too. Sales already on their way still pay. Orders
    // waiting for the truck are lost like one on its way, with what was paid for them.
    int     ordersLost = 0;
    int64_t ordersPaid = 0;
    for (const QueuedTruck& truck : state.warehouse.queue)
    {
        if (truck.job == TruckJob::Deliver)
            ++ordersLost;
        ordersPaid += truck.paid;
    }
    const int cratesLost = AbandonWarehouse(state);

    // Half the objects, at random. Things not yet built stay as jobs.
    std::vector<int> built;
    for (const LairObject& object : state.lair.objects)
        if (object.built)
            built.push_back(object.id);
    const int builtCount  = (int)built.size();
    const int objectsLost = (int)std::ceil((float)builtCount * kRaidObjectLoss);
    std::vector<std::string> lostNames;
    for (int i = 0; i < objectsLost; ++i)
    {
        const size_t pick = state.random() % built.size();
        for (const LairObject& object : state.lair.objects)
            if (object.id == built[pick])
                lostNames.push_back(GetLairObjectTypes()[(size_t)object.type].name);
        RemoveLairObjectForRaid(state, built[pick]);
        built.erase(built.begin() + (std::ptrdiff_t)pick);
    }

    // The men: anyone still in the building may be taken. The others turn up at the new place.
    std::vector<std::string> arrested;
    int placed = 0;
    for (CrewMember& member : state.crew)
    {
        if (state.city.FindMissionOf(member.id))
        {
            member.away = true; // he was already on his way out: straight to the car
            continue;
        }
        if (state.city.FindOut(member.id))
            continue; // already wounded or locked up

        if (member.role != CrewRole::Boss && (int)(state.random() % 100) < kRaidArrestChance)
        {
            CrewOut out;
            out.crewId      = member.id;
            out.arrested    = true;
            out.minutesLeft = kRaidArrestHours * 60.0f;
            state.city.out.push_back(out);
            member.away  = true;
            member.jobId = CitySim::kMissionJobId;
            arrested.push_back(member.name);
            continue;
        }

        // In by the door, side by side. The boss goes down to his place by himself.
        member.away         = false;
        member.jobId        = 0;
        member.idleTime     = 0.5f + 0.5f * (float)placed;
        member.motion.speed = CharacterMotion{}.speed;
        TileCoord spot = kFrontInside;
        spot.x += placed % 5 - 2;
        spot.z += placed / 5;
        if (!state.world.IsWalkable(spot))
            spot = kFrontInside;
        PlaceCharacter(member.motion, state.world, spot);
        ++placed;
    }
    state.lair.tasks.clear();
    RefreshLairReachable(state);

    state.heat     = 0.0f;
    evidence       = 0.0f;
    m_visitMinutes = -1.0f;
    ClearAgents();

    // What the screen says.
    char line[256];
    report.lines.push_back("Don Carlo got out by the bolt-hole, and the family has moved to a new warehouse. The layout is the same: every dug tile, wall, door and room.");
    if (safe > 0)
        snprintf(line, sizeof(line), "Money: lost %s of %s (%d%% of what was loose, %d%% of the %s safe in the vault).", WarehouseMoneyText(moneyLost).c_str(),
                 WarehouseMoneyText(before).c_str(), (int)(kRaidLooseMoneyLoss * 100.0f + 0.5f), (int)(kRaidVaultMoneyLoss * 100.0f + 0.5f), WarehouseMoneyText(safe).c_str());
    else
        snprintf(line, sizeof(line), "Money: lost %s of %s (%d%%). With a Vault it would have been less.", WarehouseMoneyText(moneyLost).c_str(), WarehouseMoneyText(before).c_str(),
                 (int)(kRaidLooseMoneyLoss * 100.0f + 0.5f));
    report.lines.push_back(line);
    report.lines.push_back(cratesLost > 0 ? "Crates: all " + std::to_string(cratesLost) + " lost." : "Crates: none to lose.");
    if (ordersLost > 0)
    {
        snprintf(line, sizeof(line), "Orders: %d in the queue called off, and the %s paid for them is gone.", ordersLost, WarehouseMoneyText(ordersPaid).c_str());
        report.lines.push_back(line);
    }
    if (objectsLost > 0)
    {
        std::sort(lostNames.begin(), lostNames.end());
        report.lines.push_back("Objects: lost " + std::to_string(objectsLost) + " of " + std::to_string(builtCount) + ": " + JoinNames(lostNames) + ".");
    }
    else
    {
        report.lines.push_back("Objects: nothing to lose.");
    }
    if (!arrested.empty())
    {
        snprintf(line, sizeof(line), "Men: %s %s arrested, out for %d hours.", JoinNames(arrested).c_str(), arrested.size() > 1 ? "were" : "was", (int)kRaidArrestHours);
        report.lines.push_back(line);
    }
    else
    {
        report.lines.push_back("Men: nobody was arrested.");
    }
    report.lines.push_back("Heat and evidence are back to 0: the police have lost the trail.");
    report.lines.push_back(EscapesLeft() == 1 ? "1 escape left." : "No escapes left: the next raid catches the boss.");

    screen = LawScreen::Escaped;
    state.PostEvent(GameEventKind::Info, "The family has moved to a new warehouse.");
    devLog.push_back("escape " + std::to_string(raidsSurvived) + " day " + std::to_string(state.clock.GetDay()) + " " + std::to_string(state.clock.GetHour()) + "h lostmoney=" +
                     std::to_string(moneyLost) + " crates=" + std::to_string(cratesLost) + " objects=" + std::to_string(objectsLost) + "/" + std::to_string(builtCount) +
                     " arrested=" + std::to_string(arrested.size()));
}

void LawSim::BustBoss(GameState& state)
{
    report = {};
    report.title = "Busted";
    const int days = state.clock.GetDay();
    char line[256];
    report.lines.push_back("The third raid found Don Carlo at home. There was no bolt-hole left to run to, and the family is finished.");
    snprintf(line, sizeof(line), "Days survived: %d", days);
    report.lines.push_back(line);
    snprintf(line, sizeof(line), "Money earned: %s (%s from crates, %s from missions)", WarehouseMoneyText(state.warehouse.moneyEarned + state.city.moneyEarned).c_str(),
             WarehouseMoneyText(state.warehouse.moneyEarned).c_str(), WarehouseMoneyText(state.city.moneyEarned).c_str());
    report.lines.push_back(line);
    snprintf(line, sizeof(line), "Missions: %d started, %d won", state.city.missionsStarted, state.city.missionsWon);
    report.lines.push_back(line);
    snprintf(line, sizeof(line), "Raids: %d. Investigators: %d, with %d reports.", raidsSurvived + 1, investigatorsSeen, reportsFiled);
    report.lines.push_back(line);
    screen = LawScreen::Busted;
    devLog.push_back("busted day " + std::to_string(days) + " " + std::to_string(state.clock.GetHour()) + "h");
}

void LawSim::DismissScreen(GameState& state)
{
    if (screen == LawScreen::Busted)
    {
        newGameWanted = true;
        return;
    }
    screen = LawScreen::None;
    if (speedBeforeRaid > 0)
        state.clock.SetSpeed(speedBeforeRaid);
    speedBeforeRaid = 0;
}

void LawSim::Tick(GameState& state, const SimStep& step)
{
    TickSlowChanges(state, step);
    TickVisits(state, step);
    TickAgents(state, step);
    TickRaid(state, step);
}
