#include "city_dev.h"

#if defined(GAME_DEV_TOOLS_ENABLED)

#include "../sim/game_state.h"

#include "../world/world_layout.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>

namespace
{
    constexpr float kCarefulHeatLimit = 30.0f; // the careful player stops working above this
    constexpr float kCarefulBribeHeat = 25.0f; // and buys it down from here
    constexpr int   kCratesWorthShipping = 6;  // the smuggler sends a load off from here
    constexpr float kMixedHeatLimit = 50.0f;   // the mixed player stops missions here, and keeps smuggling
    constexpr float kBunksFromEnergy = 70.0f;  // every auto player starts on its bunks when a man falls to this
    constexpr int   kBunksMoneyToStart = 1500; // and it can spare this
    constexpr int   kMoneyForABed = 700;       // what it keeps in hand to add a bed
    constexpr int   kMenPerBed = 4;            // a bed turns a man round in 3.5 hours of his day: this leaves some over
    constexpr int   kAutoBedCount = 6;         // what its room holds

    const char* PhaseName(MissionPhase phase)
    {
        switch (phase)
        {
        case MissionPhase::Leaving: return "leaving";
        case MissionPhase::DrivingOut: return "drivingout";
        case MissionPhase::Working: return "working";
        case MissionPhase::DrivingBack: return "drivingback";
        }
        return "?";
    }

    const char* StatusName(CrewStatus status)
    {
        switch (status)
        {
        case CrewStatus::Boss: return "boss";
        case CrewStatus::Free: return "free";
        case CrewStatus::Working: return "working";
        case CrewStatus::OnMission: return "mission";
        case CrewStatus::Wounded: return "wounded";
        case CrewStatus::Arrested: return "arrested";
        case CrewStatus::Tired: return "tired";
        case CrewStatus::Resting: return "resting";
        }
        return "?";
    }
}

std::string ExecuteCityCommand(GameState& state, std::istringstream& in)
{
    std::string word;
    in >> word;
    CitySim& city = state.city;
    const auto defs = GetMissionDefs();
    std::ostringstream out;
    out << "ok";

    if (word == "markers")
    {
        const double now = state.clock.GetTotalMinutes();
        for (const CityMarker& marker : city.markers)
        {
            const CitySite& site = kCitySites[marker.site];
            out << " " << marker.id << ":" << defs[(size_t)marker.def].key << ":" << site.name << ":"
                << (int)(city.screenOffsetX + site.x * city.screenScale) << ":" << (int)(city.screenOffsetY + site.y * city.screenScale) << ":"
                << (int)((marker.expiresAt - now) / 60.0);
        }
    }
    else if (word == "spawn")
    {
        std::string key;
        in >> key;
        for (size_t i = 0; i < defs.size(); ++i)
        {
            if (key == defs[i].key)
            {
                if (!city.SpawnMarker(state, (int)i))
                    return "err no free site";
                out << " " << city.markers.back().id;
                return out.str();
            }
        }
        return "err no such mission";
    }
    else if (word == "go")
    {
        uint32_t marker = 0;
        in >> marker;
        std::vector<uint32_t> men;
        uint32_t              id = 0;
        while (in >> id)
            men.push_back(id);
        if (!city.StartMission(state, marker, men))
            return "err can't start that";
    }
    else if (word == "hire")
    {
        uint32_t marker = 0;
        in >> marker;
        if (!city.HireCandidate(state, marker))
            return "err can't hire";
    }
    else if (word == "crew")
    {
        for (const CrewMember& member : state.crew)
            out << " " << member.id << ":" << member.name << ":" << StatusName(city.GetCrewStatus(member)) << ":" << member.muscle << ":" << member.brains << ":"
                << member.nerve << ":" << member.jobId << ":" << (member.away ? 1 : 0);
    }
    else if (word == "missions")
    {
        for (const ActiveMission& mission : city.missions)
        {
            Vec2 position, direction;
            mission.SampleCar(position, direction);
            out << " " << mission.id << ":" << defs[(size_t)mission.def].key << ":" << PhaseName(mission.phase) << ":";
            for (uint32_t id : mission.crew)
                out << id << ",";
            out << ":" << mission.chance << ":" << (int)position.x << "," << (int)position.y;
        }
    }
    else if (word == "job")
    {
        uint32_t id  = 0;
        int      job = 0;
        in >> id >> job;
        CrewMember* member = state.FindCrew(id);
        if (!member)
            return "err no such crew member";
        member->jobId = job;
    }
    else if (word == "buttons")
    {
        out << " popup=" << city.devPopupButtonX << ":" << city.devPopupButtonY << " card=" << city.devCardButtonX << ":" << city.devCardButtonY;
    }
    else if (word == "results")
    {
        out << " results=" << city.results.size();
    }
    else if (word == "totals")
    {
        out << " started=" << city.missionsStarted << " won=" << city.missionsWon << " lost=" << city.missionsLost << " earned=" << city.moneyEarned
            << " spent=" << city.moneySpent << " heatadded=" << city.heatFromMissions << " out=" << city.out.size() << " crew=" << state.crew.size();
    }
    else if (word == "auto")
    {
        in >> city.devAutoPlayer;
    }
    else
    {
        return "err unknown city command";
    }
    return out.str();
}

namespace
{
    // Plays the warehouse: sells when it holds a load, buys the dearest kind it can afford
    // (contraband only if `contraband`, as it brings the most heat). It only orders when the truck is
    // free and never queues, so the balance numbers measured with it don't depend on the queue.
    void RunSmuggler(GameState& state, bool contraband)
    {
        if (state.warehouse.phase != TruckPhase::Gone)
            return;
        if (state.crates >= kCratesWorthShipping)
        {
            OrderShipOut(state);
            return;
        }
        for (int kind = GetShipmentKindCount() - (contraband ? 1 : 2); kind >= 0; --kind)
            if (CanOrderShipment(state, kind) && OrderShipment(state, kind))
                return;
    }
}

namespace
{
    // A Bunks room beside the basement, 4 x 3 with a door in the wall they share and a walkway down
    // its middle: dig it, wall it, then a bed per kMenPerBed men. Beds lost to a raid come back.
    void RunBunkBuilder(GameState& state)
    {
        if (state.law.phase != RaidPhase::None)
            return;

        const LairRect rect = { kStairFoot.x + 3, kStairFoot.z + 1, kStairFoot.x + 6, kStairFoot.z + 3 };
        const auto findType = [](auto types, const char* key) {
            for (size_t i = 0; i < types.size(); ++i)
                if (strcmp(types[i].key, key) == 0)
                    return (int)i;
            return -1;
        };
        const int roomType = findType(GetLairRoomTypes(), "bunks");
        const int bedType  = findType(GetLairObjectTypes(), "bunk");

        const LairRoom* room = FindLairRoomAt(state, { WorldLevel::Underground, rect.x0, rect.z0 });
        if (!room)
        {
            // Not yet needed, or can't be spared.
            bool needed = false;
            for (const CrewMember& member : state.crew)
                needed = needed || (member.role != CrewRole::Boss && member.energy < kBunksFromEnergy);
            if (!needed || state.money < kBunksMoneyToStart)
                return;

            bool dug = true;
            for (int z = rect.z0; z <= rect.z1; ++z)
            {
                for (int x = rect.x0; x <= rect.x1; ++x)
                {
                    const TileCoord tile = { WorldLevel::Underground, x, z };
                    if (state.world.GetTile(tile).kind == TileKind::Floor)
                        continue;
                    dug = false;
                    MarkLairDig(state, tile);
                }
            }
            if (dug)
                PlaceLairRoom(state, roomType, rect);
            return;
        }

        const LairEdge door = MakeLairEdge({ WorldLevel::Underground, rect.x0 - 1, rect.z0 + 1 }, EdgeSide::PosX);
        if (!FindLairDoor(state, door))
        {
            PlaceLairDoor(state, door);
            return;
        }

        int soldiers = 0;
        for (const CrewMember& member : state.crew)
            soldiers += member.role != CrewRole::Boss ? 1 : 0;
        const int wanted = std::min((soldiers + kMenPerBed - 1) / kMenPerBed, kAutoBedCount);

        int beds = 0;
        for (const LairObject& object : state.lair.objects)
            beds += object.type == bedType ? 1 : 0;
        if (beds >= wanted || state.money < kMoneyForABed)
            return;
        for (int z : { rect.z0, rect.z1 })
        {
            for (int x = rect.x0 + 1; x <= rect.x1; ++x)
            {
                const TileCoord tile = { WorldLevel::Underground, x, z };
                if (!FindLairObjectAt(state, tile) && PlaceLairObject(state, bedType, tile, 0).ok)
                    return;
            }
        }
    }
}

void RunCityAutoPlayer(GameState& state, int mode)
{
    RunBunkBuilder(state);
    if (mode == 6)
        return; // only the bunks, for testing

    // 1 greedy missions, 2 careful missions, 3 only smuggling up to booze, 4 smuggling and
    // missions, 5 only smuggling, contraband when it can.
    if (mode >= 3)
        RunSmuggler(state, mode != 3);
    if (mode == 3 || mode == 5)
        return;

    CitySim&   city = state.city;
    const auto defs = GetMissionDefs();

    // Copies: a mission taken comes off the list.
    const std::vector<CityMarker> markers = city.markers;
    for (const CityMarker& marker : markers)
    {
        const MissionDef& def = defs[(size_t)marker.def];
        if (def.effect == MissionEffect::Hire)
        {
            if (mode != 2 && state.money > 6000)
                city.HireCandidate(state, marker.id);
            continue;
        }
        const bool isBribe = def.heat < 0.0f;
        if (mode != 2 && isBribe)
            continue; // the greedy player doesn't pay to cool off
        if (mode == 2)
        {
            if (isBribe ? state.heat < kCarefulBribeHeat : state.heat >= kCarefulHeatLimit)
                continue;
        }
        const std::vector<uint32_t> men = city.DefaultPick(state, def);
        if (mode == 4)
        {
            // Mixed: leaves men at work alone, as a person would, and stops when it's too hot.
            bool working = state.heat >= kMixedHeatLimit;
            for (uint32_t id : men)
                working = working || state.FindCrew(id)->jobId != 0;
            if (working)
                continue;
        }
        if (!city.WhyNotGo(state, def, men))
            city.StartMission(state, marker.id, men);
    }
}

#endif
