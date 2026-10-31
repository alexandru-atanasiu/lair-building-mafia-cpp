#include "city_dev.h"

#if defined(GAME_DEV_TOOLS_ENABLED)

#include "../sim/game_state.h"

#include <cstdio>
#include <iterator>

namespace
{
    constexpr float kCarefulHeatLimit = 30.0f; // the careful player stops working above this
    constexpr float kCarefulBribeHeat = 25.0f; // and buys it down from here
    constexpr int   kCratesWorthShipping = 6;  // the smuggler sends a load off from here
    constexpr float kMixedHeatLimit = 50.0f;   // the mixed player stops missions here, and keeps smuggling

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
    // (contraband only if `contraband`, as it brings the most heat).
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

void RunCityAutoPlayer(GameState& state, int mode)
{
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
