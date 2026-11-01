#include "rest_dev.h"

#if defined(GAME_DEV_TOOLS_ENABLED)

#include "../sim/game_state.h"

#include <algorithm>

namespace
{
    const char* StatusName(const GameState& state, const CrewMember& member)
    {
        switch (state.city.GetCrewStatus(member))
        {
        case CrewStatus::Boss: return "boss";
        case CrewStatus::Free: return "free";
        case CrewStatus::Working: return "working";
        case CrewStatus::OnMission: return "mission";
        case CrewStatus::Wounded: return "wounded";
        case CrewStatus::Arrested: return "arrested";
        case CrewStatus::Tired: return "tired";
        case CrewStatus::Resting: return member.inBed ? "resting" : "tobed";
        }
        return "?";
    }
}

std::string ExecuteRestCommand(GameState& state, std::istringstream& in)
{
    std::string word;
    in >> word;
    std::ostringstream out;
    out << "ok";

    if (word.empty())
    {
        out << " beds=" << CountBeds(state) << " free=" << CountFreeBeds(state) << " off=" << CountMenOffWork(state) << " inbed=" << CountMenInBed(state);
        for (const LairObject& object : state.lair.objects)
        {
            if (!object.built || !GetLairObjectTypes()[(size_t)object.type].bed)
                continue;
            uint32_t user = 0;
            for (const CrewMember& member : state.crew)
                if (member.restBed == object.id)
                    user = member.id;
            out << " bed" << object.id << "=" << object.tile.x << ":" << object.tile.z << ":" << user;
        }
        for (const CrewMember& member : state.crew)
            out << " man" << member.id << "=" << member.name << ":" << member.energy << ":" << StatusName(state, member) << ":" << member.restBed;
    }
    else if (word == "set")
    {
        std::string who;
        float       energy = 0.0f;
        if (!(in >> who >> energy))
            return "err expected: rest set <crew id|all> <energy>";
        energy = std::clamp(energy, 0.0f, kEnergyFull);
        bool found = false;
        for (CrewMember& member : state.crew)
        {
            if (member.role == CrewRole::Boss || (who != "all" && who != std::to_string(member.id)))
                continue;
            member.energy = energy;
            found = true;
        }
        if (!found)
            return "err no such soldier";
    }
    else
    {
        return "err unknown rest command";
    }
    return out.str();
}

#endif
