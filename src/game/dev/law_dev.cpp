#include "law_dev.h"

#if defined(GAME_DEV_TOOLS_ENABLED)

#include "../sim/game_state.h"

#include <algorithm>

namespace
{
    const char* PhaseName(RaidPhase phase)
    {
        return phase == RaidPhase::None ? "none" : phase == RaidPhase::Warning ? "warning" : "police";
    }

    const char* ScreenName(LawScreen screen)
    {
        return screen == LawScreen::None ? "none" : screen == LawScreen::Escaped ? "escaped" : "busted";
    }

    // The rules of "The loop and the crew" in CLAUDE.md, for a script to poll.
    std::string CheckCrew(const GameState& state)
    {
        std::string problems;
        for (const CrewMember& member : state.crew)
        {
            const bool onMission = state.city.FindMissionOf(member.id) != nullptr;
            const bool isOut     = state.city.FindOut(member.id) != nullptr;
            const bool bossGone  = member.role == CrewRole::Boss && member.jobId == kLawJobId && state.law.phase == RaidPhase::Police;
            if (member.role == CrewRole::Boss && member.jobId != 0 && !bossGone && state.law.screen == LawScreen::None)
                problems += " " + member.name + ":boss-has-job";
            if (member.away && !onMission && !isOut && !bossGone)
                problems += " " + member.name + ":away-for-nothing";
            if (onMission && isOut)
                problems += " " + member.name + ":mission-and-out";
            if (member.jobId == CitySim::kMissionJobId && !onMission && !isOut)
                problems += " " + member.name + ":mission-job-without-mission";
            if (member.jobId >= kLairFirstJobId && member.away)
                problems += " " + member.name + ":away-with-lair-job";
            int carrying = 0;
            for (const CrateCarrier& carrier : state.warehouse.carriers)
                carrying += carrier.crewId == member.id ? 1 : 0;
            if (carrying > 1)
                problems += " " + member.name + ":carrying-twice";
            int missions = 0;
            for (const ActiveMission& mission : state.city.missions)
                missions += (int)std::count(mission.crew.begin(), mission.crew.end(), member.id);
            if (missions > 1)
                problems += " " + member.name + ":on-two-missions";
            int outs = 0;
            for (const CrewOut& entry : state.city.out)
                outs += entry.crewId == member.id ? 1 : 0;
            if (outs > 1)
                problems += " " + member.name + ":out-twice";
        }
        // Nobody walks through a fixture: not the racks, and not the shelves hiding the back room.
        const auto inFixture = [&](const std::string& name, const CharacterMotion& motion) {
            if (state.world.GetTile(state.world.WorldToTile(motion.level, motion.position)).kind == TileKind::Fixture)
                problems += " " + name + ":in-a-fixture";
        };
        for (const CrewMember& member : state.crew)
            if (!member.away)
                inFixture(member.name, member.motion);
        for (const LawAgent& agent : state.law.agents)
            inFixture(agent.name, agent.motion);
        if ((int)state.warehouse.stored.size() != state.crates)
            problems += " crates-out-of-step";
        return problems;
    }
}

std::string ExecuteLawCommand(GameState& state, std::istringstream& in)
{
    LawSim&     law = state.law;
    std::string word;
    in >> word;
    std::ostringstream out;
    out << "ok";

    if (word.empty())
    {
        out << " evidence=" << law.evidence << " heat=" << state.heat << " phase=" << PhaseName(law.phase) << " warningminutes=" << law.warningMinutes
            << " escapes=" << law.EscapesLeft() << " raids=" << law.raidsSurvived << " screen=" << ScreenName(law.screen) << " button=" << law.screenButtonX << ":"
            << law.screenButtonY << " investigators=" << law.CountAgents(LawAgentKind::Investigator) << " officers=" << law.CountAgents(LawAgentKind::Officer)
            << " reports=" << law.reportsFiled << " inside=" << (law.policeInside ? 1 : 0);
        for (const LawAgent& agent : law.agents)
        {
            const TileCoord tile = state.world.WorldToTile(agent.motion.level, agent.motion.position);
            out << " agent" << agent.id << "=" << (agent.kind == LawAgentKind::Officer ? "officer" : "investigator") << ":"
                << (agent.motion.level == WorldLevel::Ground ? "ground" : "under") << ":" << tile.x << ":" << tile.z << ":" << (agent.motion.IsMoving() ? "walking" : "still")
                << ":" << agent.seenCrates.size();
        }
    }
    else if (word == "evidence")
    {
        float evidence = 0.0f;
        in >> evidence;
        law.evidence = 0.0f;
        law.AddEvidence(evidence);
    }
    else if (word == "investigator")
    {
        int count = 1;
        in >> count;
        law.SpawnInvestigators(state, count);
    }
    else if (word == "raid" || word == "raidnow" || word == "third")
    {
        if (law.phase != RaidPhase::None)
            return "err a raid is already on";
        if (word == "third")
            law.raidsSurvived = kLawEscapes;
        law.StartRaid(state, word == "raidnow");
    }
    else if (word == "dismiss")
    {
        if (law.screen == LawScreen::None)
            return "err no screen up";
        law.DismissScreen(state);
    }
    else if (word == "report")
    {
        out << " " << law.report.title;
        for (const std::string& line : law.report.lines)
            out << " | " << line;
    }
    else if (word == "autodismiss")
    {
        int on = 1;
        in >> on;
        law.devAutoDismiss = on != 0;
    }
    else if (word == "turbo")
    {
        in >> law.devTurbo;
        law.devTurbo = std::clamp(law.devTurbo, 1, 200);
    }
    else if (word == "check")
    {
        const std::string problems = CheckCrew(state);
        if (!problems.empty())
            return "err" + problems;
    }
    else if (word == "log")
    {
        for (const std::string& line : law.devLog)
            out << " | " << line;
    }
    else
    {
        return "err unknown law command";
    }
    return out.str();
}

#endif
