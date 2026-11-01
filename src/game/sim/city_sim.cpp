#include "city_sim.h"

#include "game_state.h"

#include "../world/world_layout.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <limits>
#include <queue>

#if defined(GAME_DEV_TOOLS_ENABLED)
#include "../dev/city_dev.h"
#endif

namespace
{
    // ---- Tuning: the rest of the numbers are in kMissions below ----

    // Chance of success
    constexpr int   kAverageStat         = 3;  // the stat at which a mission gives its baseChance
    constexpr int   kChancePerStatPoint  = 10; // % per point of the men's average above or below it
    constexpr int   kChancePerExtraMan   = 6;  // % for each man past the first
    constexpr int   kMinChance           = 5;
    constexpr int   kMaxChance           = 95;

    // Failure, for each man. Nerve lowers both.
    constexpr int   kWoundChanceBase     = 40;
    constexpr int   kWoundChancePerNerve = 6;
    constexpr int   kArrestChanceBase    = 26;
    constexpr int   kArrestChancePerNerve = 4;
    constexpr float kWoundedHours        = 16.0f;
    constexpr float kArrestedHours       = 48.0f;

    // Heat falls once a game hour, slower while men are out on missions.
    constexpr float kHeatDecayPerHour        = 0.5f;
    constexpr float kHeatDecayPerHourMenOut  = 0.2f;

    // Driving: a game hour is 5 real seconds at 1x, and the map is 2048 pixels across.
    constexpr float kDrivePixelsPerHour = 600.0f;

    // Markers
    constexpr float kMarkerEveryHours    = 2.5f; // one more on average, with +-50% jitter
    constexpr int   kMaxMarkers          = 9;
    constexpr float kExpiryJitter        = 0.3f; // +-30% of a mission's markerHours

    // ---- The missions: one row each ----

    const Color kMarkerMoney = { 0.82f, 0.68f, 0.32f, 1.0f };
    const Color kMarkerBold  = { 0.80f, 0.48f, 0.28f, 1.0f };
    const Color kMarkerWater = { 0.42f, 0.66f, 0.76f, 1.0f };
    const Color kMarkerQuiet = { 0.62f, 0.72f, 0.48f, 1.0f };
    const Color kMarkerFix   = { 0.70f, 0.62f, 0.82f, 1.0f };
    const Color kMarkerMan   = { 0.88f, 0.86f, 0.78f, 1.0f };

    const MissionDef kMissions[] = {
        //  key          name               blurb
        //  icon, color, location, effect, stat,
        //  min/max men, hours, chance%, investment, reward, heat, failHeat, weight, markerHours, maxLive
        { "shakedown", "Shakedown", "Lean on the owner until protection starts to sound like a good idea.",
          "ui/city/markers/shakedown.tex", kMarkerBold, CityLocation::Shop, MissionEffect::Job, CrewStat::Muscle,
          1, 1, 3.0f, 70, 0, 500, 3.0f, 5.0f, 3.0f, 14.0f, 3 },
        { "burglary", "Burglary", "In through a back window while the family is at the shore.",
          "ui/city/markers/burglary.tex", kMarkerQuiet, CityLocation::Residence, MissionEffect::Job, CrewStat::Brains,
          1, 1, 5.0f, 65, 100, 1200, 5.0f, 8.0f, 2.0f, 16.0f, 2 },
        { "smuggling", "Smuggling run", "A boat, a crew who ask no questions, cash up front. Slow and safe.",
          "ui/city/markers/smuggling.tex", kMarkerWater, CityLocation::Dock, MissionEffect::Job, CrewStat::Nerve,
          1, 2, 8.0f, 75, 500, 2000, 3.0f, 6.0f, 2.0f, 20.0f, 2 },
        { "hijack", "Hijack a truck", "A driver takes a long look at a gun, and the load goes missing.",
          "ui/city/markers/hijack.tex", kMarkerBold, CityLocation::TruckRoute, MissionEffect::Job, CrewStat::Muscle,
          1, 2, 6.0f, 60, 200, 2200, 7.0f, 10.0f, 1.5f, 12.0f, 2 },
        { "robbery", "Robbery", "The big one: in the front door with the guns out. Takes more than one man.",
          "ui/city/markers/robbery.tex", kMarkerMoney, CityLocation::Bank, MissionEffect::Job, CrewStat::Nerve,
          2, 3, 8.0f, 50, 800, 6000, 14.0f, 18.0f, 0.7f, 18.0f, 1 },
        { "bribe", "Bribe", "An envelope changes hands, and a file goes missing. Costs plenty, and lowers heat and evidence.",
          "ui/city/markers/bribe.tex", kMarkerFix, CityLocation::Office, MissionEffect::Job, CrewStat::Brains,
          1, 1, 2.0f, 80, 1500, 0, -18.0f, 4.0f, 0.8f, 24.0f, 1, -12.0f },
        { "recruit", "Recruit", "A man looking for work. Pay what it takes to bring him in.",
          "ui/city/markers/recruit.tex", kMarkerMan, CityLocation::Bar, MissionEffect::Hire, CrewStat::Nerve,
          0, 0, 0.0f, 0, 1000, 0, 0.0f, 0.0f, 1.2f, 30.0f, 1 },
    };

    struct StartingStats
    {
        const char* name;
        int         muscle, brains, nerve;
    };
    const StartingStats kStartingStats[] = {
        { "Don Carlo", 3, 4, 5 },
        { "Sal", 4, 2, 3 },
        { "Vinnie", 2, 4, 3 },
        { "Paulie", 3, 3, 4 },
        { "Tony", 5, 1, 2 },
        { "Frankie", 2, 3, 5 },
    };

    const char* const kRecruitNames[] = {
        "Enzo", "Rocco", "Dominic", "Nino", "Gino", "Luca", "Mikey", "Bruno", "Angelo", "Carmine", "Dante", "Marco", "Aldo", "Silvio", "Tommy", "Joey",
    };
    // Dull suits: the boss is the only saturated color.
    const Color kRecruitTints[] = {
        { 0.26f, 0.26f, 0.34f, 1.0f }, { 0.36f, 0.32f, 0.26f, 1.0f }, { 0.30f, 0.34f, 0.28f, 1.0f }, { 0.40f, 0.40f, 0.42f, 1.0f },
        { 0.24f, 0.30f, 0.34f, 1.0f }, { 0.32f, 0.28f, 0.32f, 1.0f }, { 0.28f, 0.26f, 0.22f, 1.0f }, { 0.44f, 0.42f, 0.36f, 1.0f },
    };

    std::string JoinNames(const GameState& state, const std::vector<uint32_t>& ids)
    {
        std::string names;
        for (size_t i = 0; i < ids.size(); ++i)
        {
            for (const CrewMember& member : state.crew)
                if (member.id == ids[i])
                    names += member.name;
            if (i + 2 < ids.size())
                names += ", ";
            else if (i + 2 == ids.size())
                names += " and ";
        }
        return names;
    }

    std::string Money(int amount)
    {
        std::string digits = std::to_string(amount);
        for (int i = (int)digits.size() - 3; i > 0; i -= 3)
            digits.insert((size_t)i, ",");
        return "$" + digits;
    }

    // ---- Roads ----

    Vec2 NodePos(int node) { return { kCityRoadNodes[node].x, kCityRoadNodes[node].y }; }

    // The shortest way between two road nodes, as node indices.
    std::vector<int> FindRoadPath(int from, int to)
    {
        const int          count = (int)std::size(kCityRoadNodes);
        std::vector<float> dist((size_t)count, std::numeric_limits<float>::max());
        std::vector<int>   previous((size_t)count, -1);
        using Entry = std::pair<float, int>;
        std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
        dist[(size_t)from] = 0.0f;
        open.push({ 0.0f, from });
        while (!open.empty())
        {
            const auto [d, node] = open.top();
            open.pop();
            if (node == to)
                break;
            if (d > dist[(size_t)node])
                continue;
            for (const CityRoadEdge& edge : kCityRoadEdges)
            {
                const int next = edge.a == node ? edge.b : (edge.b == node ? edge.a : -1);
                if (next < 0)
                    continue;
                const float dx = kCityRoadNodes[next].x - kCityRoadNodes[node].x;
                const float dy = kCityRoadNodes[next].y - kCityRoadNodes[node].y;
                const float nd = d + std::sqrt(dx * dx + dy * dy);
                if (nd < dist[(size_t)next])
                {
                    dist[(size_t)next]     = nd;
                    previous[(size_t)next] = node;
                    open.push({ nd, next });
                }
            }
        }
        std::vector<int> path;
        for (int node = to; node >= 0; node = previous[(size_t)node])
            path.push_back(node);
        std::reverse(path.begin(), path.end());
        return path;
    }

    std::vector<Vec2> MakeRoute(const CitySite& site)
    {
        std::vector<Vec2> route;
        route.push_back({ kCityWarehouse.stopX, kCityWarehouse.stopY });
        for (int node : FindRoadPath(kCityWarehouse.node, site.node))
            route.push_back(NodePos(node));
        route.push_back({ site.stopX, site.stopY });
        return route;
    }

    float RouteLength(const std::vector<Vec2>& route)
    {
        float length = 0.0f;
        for (size_t i = 1; i < route.size(); ++i)
        {
            const float dx = route[i].x - route[i - 1].x;
            const float dy = route[i].y - route[i - 1].y;
            length += std::sqrt(dx * dx + dy * dy);
        }
        return length;
    }

    // ---- The warehouse's door ----

    // The door out of the warehouse: a door edge between a Floor tile and Pavement. Found in the
    // world rather than assumed, as the warehouse may change.
    bool FindWarehouseDoor(const WorldGrid& world, TileCoord& outside, TileCoord& inside)
    {
        static const struct { EdgeSide side; int dx, dz; } kSides[] = {
            { EdgeSide::NegZ, 0, -1 }, { EdgeSide::PosZ, 0, 1 }, { EdgeSide::NegX, -1, 0 }, { EdgeSide::PosX, 1, 0 },
        };
        for (int z = 0; z < world.GetDepth(); ++z)
        {
            for (int x = 0; x < world.GetWidth(); ++x)
            {
                const TileCoord tile = { WorldLevel::Ground, x, z };
                if (world.GetTile(tile).kind != TileKind::Floor)
                    continue;
                for (const auto& s : kSides)
                {
                    const TileCoord next = { WorldLevel::Ground, x + s.dx, z + s.dz };
                    if (world.GetEdge(tile, s.side) == EdgeKind::Door && world.GetTile(next).kind == TileKind::Pavement)
                    {
                        outside = next;
                        inside  = tile;
                        return true;
                    }
                }
            }
        }
        return false;
    }

    // Takes a man off whatever he was doing. The job system owns jobId: whoever gave him a job
    // sees it isn't theirs any more, and puts the work back for someone else.
    void DropJob(CrewMember& member)
    {
        member.jobId = CitySim::kMissionJobId;
    }

    // Back at the door, and in through it.
    void ComeBack(GameState& state, CrewMember& member)
    {
        TileCoord outside = GetWarehouseCenter();
        TileCoord inside  = outside;
        FindWarehouseDoor(state.world, outside, inside);
        member.away     = false;
        member.jobId    = 0;
        member.idleTime = 0.0f;
        PlaceCharacter(member.motion, state.world, outside);
        SendCharacterTo(member.motion, state.world, inside);
    }

    float RandomUnit(GameState& state)
    {
        return (float)(state.random() % 1000) / 1000.0f;
    }

    int RollStat(GameState& state)
    {
        // 1..4 mostly, a 5 now and then.
        const int roll = (int)(state.random() % 12);
        return roll < 3 ? 1 : roll < 7 ? 2 : roll < 10 ? 3 : roll < 11 ? 4 : 5;
    }
}

std::span<const MissionDef> GetMissionDefs()
{
    return kMissions;
}

int GetHeatBand(float heat)
{
    return heat < 25.0f ? 0 : heat < 50.0f ? 1 : heat < 75.0f ? 2 : 3;
}

int GetHeatChancePenalty(float heat)
{
    return -10 * GetHeatBand(heat);
}

const char* GetHeatBandName(float heat)
{
    static const char* const kNames[] = { "Quiet", "Noticed", "Hot", "Scorching" };
    return kNames[GetHeatBand(heat)];
}

int GetCrewStat(const CrewMember& member, CrewStat stat)
{
    return stat == CrewStat::Muscle ? member.muscle : stat == CrewStat::Brains ? member.brains : member.nerve;
}

const char* GetCrewStatName(CrewStat stat)
{
    return stat == CrewStat::Muscle ? "Muscle" : stat == CrewStat::Brains ? "Brains" : "Nerve";
}

void ActiveMission::SampleCar(Vec2& position, Vec2& direction) const
{
    position  = route.empty() ? Vec2{} : route.front();
    direction = { 0.0f, -1.0f };
    float left = routePos;
    for (size_t i = 1; i < route.size(); ++i)
    {
        const float dx     = route[i].x - route[i - 1].x;
        const float dy     = route[i].y - route[i - 1].y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length <= 0.0f)
            continue;
        direction = { dx / length, dy / length };
        if (left <= length)
        {
            position = { route[i - 1].x + direction.x * left, route[i - 1].y + direction.y * left };
            break;
        }
        left -= length;
        position = route[i];
    }
    if (phase == MissionPhase::DrivingBack)
        direction = { -direction.x, -direction.y };
}

void CitySim::Init(GameState& state)
{
    for (CrewMember& member : state.crew)
    {
        for (const StartingStats& stats : kStartingStats)
        {
            if (member.name == stats.name)
            {
                member.muscle = stats.muscle;
                member.brains = stats.brains;
                member.nerve  = stats.nerve;
            }
        }
    }

    // Something to do from the first minute, including what's free to start.
    for (const char* key : { "shakedown", "shakedown", "burglary", "smuggling", "hijack", "recruit" })
    {
        const std::span<const MissionDef> defs = GetMissionDefs();
        for (size_t i = 0; i < defs.size(); ++i)
            if (std::string_view(defs[i].key) == key)
                SpawnMarker(state, (int)i);
    }
    m_spawnMinutes = kMarkerEveryHours * 60.0f;
    m_heatBand     = GetHeatBand(state.heat);
}

void CitySim::Tick(GameState& state, const SimStep& step)
{
    TickMarkers(state, step);
    TickMissions(state, step);
    TickOut(state, step);
    TickHeat(state, step);
#if defined(GAME_DEV_TOOLS_ENABLED)
    if (devAutoPlayer != 0)
        RunCityAutoPlayer(state, devAutoPlayer);
#endif
}

void CitySim::TickMarkers(GameState& state, const SimStep& step)
{
    const double now = state.clock.GetTotalMinutes();
    std::erase_if(markers, [now](const CityMarker& marker) { return now >= marker.expiresAt; });

    m_spawnMinutes -= step.gameMinutes;
    if (m_spawnMinutes > 0.0f)
        return;
    m_spawnMinutes = kMarkerEveryHours * 60.0f * (0.5f + RandomUnit(state));
    if ((int)markers.size() >= kMaxMarkers)
        return;

    // A row by weight, among those with room for one more.
    const std::span<const MissionDef> defs = GetMissionDefs();
    float total = 0.0f;
    std::vector<float> weights(defs.size(), 0.0f);
    for (size_t i = 0; i < defs.size(); ++i)
    {
        const int live = (int)std::count_if(markers.begin(), markers.end(), [i](const CityMarker& m) { return m.def == (int)i; });
        if (live >= defs[i].maxLive)
            continue;
        if (defs[i].effect == MissionEffect::Hire && (int)state.crew.size() >= CrewCapacity(state))
            continue;
        weights[i] = defs[i].weight;
        total += defs[i].weight;
    }
    float pick = RandomUnit(state) * total;
    for (size_t i = 0; i < defs.size(); ++i)
    {
        if (weights[i] <= 0.0f)
            continue;
        pick -= weights[i];
        if (pick <= 0.0f)
        {
            SpawnMarker(state, (int)i);
            break;
        }
    }
}

bool CitySim::SiteIsFree(int site) const
{
    for (const CityMarker& marker : markers)
        if (marker.site == site)
            return false;
    for (const ActiveMission& mission : missions)
        if (mission.site == site)
            return false;
    return true;
}

bool CitySim::SpawnMarker(GameState& state, int defIndex)
{
    const std::span<const MissionDef> defs = GetMissionDefs();
    if (defIndex < 0 || defIndex >= (int)defs.size())
        return false;
    const MissionDef& def = defs[(size_t)defIndex];

    std::vector<int> sites;
    for (int i = 0; i < (int)std::size(kCitySites); ++i)
        if (kCitySites[i].location == def.location && SiteIsFree(i))
            sites.push_back(i);
    if (sites.empty())
        return false;

    CityMarker marker;
    marker.id          = m_nextMarkerId++;
    marker.def         = defIndex;
    marker.site        = sites[state.random() % sites.size()];
    marker.lifeMinutes = def.markerHours * 60.0f * (1.0f - kExpiryJitter + 2.0f * kExpiryJitter * RandomUnit(state));
    marker.expiresAt   = state.clock.GetTotalMinutes() + marker.lifeMinutes;
    if (def.effect == MissionEffect::Hire)
        marker.candidate = MakeCandidate(state);
    markers.push_back(std::move(marker));
    return true;
}

RecruitCandidate CitySim::MakeCandidate(GameState& state) const
{
    std::vector<const char*> names;
    for (const char* name : kRecruitNames)
    {
        const bool taken = std::any_of(state.crew.begin(), state.crew.end(), [name](const CrewMember& m) { return m.name == name; });
        if (!taken)
            names.push_back(name);
    }
    RecruitCandidate candidate;
    candidate.name   = names.empty() ? "Johnny" : names[state.random() % names.size()];
    candidate.muscle = RollStat(state);
    candidate.brains = RollStat(state);
    candidate.nerve  = RollStat(state);
    candidate.tint   = kRecruitTints[state.random() % std::size(kRecruitTints)];
    return candidate;
}

void CitySim::TickMissions(GameState& state, const SimStep& step)
{
    const float driven = kDrivePixelsPerHour * step.gameMinutes / 60.0f;

    for (ActiveMission& mission : missions)
    {
        switch (mission.phase)
        {
        case MissionPhase::Leaving:
        {
            // Each man goes off the map at the door; the car leaves when the last one is out. A
            // man who can't get there (the way was built over) slips out anyway.
            bool allOut = true;
            for (uint32_t id : mission.crew)
            {
                CrewMember* member = state.FindCrew(id);
                if (!member || member->away)
                    continue;
                if (member->motion.IsMoving())
                    allOut = false;
                else
                    member->away = true;
            }
            if (allOut)
                mission.phase = MissionPhase::DrivingOut;
            break;
        }
        case MissionPhase::DrivingOut:
            mission.routePos += driven;
            if (mission.routePos >= mission.routeLength)
            {
                mission.routePos        = mission.routeLength;
                mission.phase           = MissionPhase::Working;
                mission.workMinutesLeft = GetMissionDefs()[(size_t)mission.def].hours * 60.0f;
            }
            break;
        case MissionPhase::Working:
            mission.workMinutesLeft -= step.gameMinutes;
            if (mission.workMinutesLeft <= 0.0f)
            {
                Resolve(state, mission);
                mission.phase = MissionPhase::DrivingBack;
            }
            break;
        case MissionPhase::DrivingBack:
            mission.routePos -= driven;
            if (mission.routePos <= 0.0f)
            {
                for (uint32_t id : mission.crew)
                    if (CrewMember* member = state.FindCrew(id))
                        ComeBack(state, *member);
                mission.crew.clear();
            }
            break;
        }
    }

    // Done when nobody's left on it: home, or wounded and arrested on the spot.
    std::erase_if(missions, [](const ActiveMission& mission) { return mission.crew.empty(); });
}

void CitySim::Resolve(GameState& state, ActiveMission& mission)
{
    const MissionDef& def  = GetMissionDefs()[(size_t)mission.def];
    const CitySite&   site = kCitySites[mission.site];
    const std::string men  = JoinNames(state, mission.crew);

    MissionResult result;
    result.success = (int)(state.random() % 100) < mission.chance;
    char line[256];

    if (result.success)
    {
        ++missionsWon;
        state.money += def.reward;
        moneyEarned += def.reward;
        state.AddHeat(def.heat);
        state.law.AddEvidence(def.evidence);
        if (def.heat > 0.0f)
            heatFromMissions += def.heat;

        result.title = std::string(def.name) + ": success";
        if (def.reward > 0)
            snprintf(line, sizeof(line), "%s pulled it off at %s. +%s, heat +%d.", men.c_str(), site.name, Money(def.reward).c_str(), (int)(def.heat + 0.5f));
        else
            snprintf(line, sizeof(line), "%s squared it at %s. Heat %d, evidence %d.", men.c_str(), site.name, (int)(def.heat - 0.5f), (int)(def.evidence - 0.5f));
        result.text = line;
    }
    else
    {
        ++missionsLost;
        state.AddHeat(def.failHeat);
        heatFromMissions += def.failHeat;

        // Who pays for it. Whoever's left drives home.
        std::vector<uint32_t> wounded, arrested, unhurt;
        for (uint32_t id : mission.crew)
        {
            CrewMember* member = state.FindCrew(id);
            if (!member)
                continue;
            const int roll = (int)(state.random() % 100);
            CrewOut   hurt;
            hurt.crewId = id;
            if (roll < ArrestChance(member->nerve))
            {
                hurt.arrested    = true;
                hurt.minutesLeft = kArrestedHours * 60.0f;
                arrested.push_back(id);
                out.push_back(hurt);
            }
            else if (roll < ArrestChance(member->nerve) + WoundChance(member->nerve))
            {
                hurt.minutesLeft = kWoundedHours * 60.0f;
                wounded.push_back(id);
                out.push_back(hurt);
            }
            else
            {
                unhurt.push_back(id);
            }
        }
        mission.crew = unhurt;

        result.title = std::string(def.name) + ": failed";
        std::string text = "It went wrong at " + std::string(site.name) + ". Lost " + Money(def.investment) + ", heat +" + std::to_string((int)(def.failHeat + 0.5f)) + ".";
        if (!unhurt.empty())
            text += " " + JoinNames(state, unhurt) + " got away.";
        if (!wounded.empty())
            text += " " + JoinNames(state, wounded) + (wounded.size() > 1 ? " are" : " is") + " wounded, out for " + std::to_string((int)kWoundedHours) + " hours.";
        if (!arrested.empty())
            text += " " + JoinNames(state, arrested) + (arrested.size() > 1 ? " were" : " was") + " arrested, out for " + std::to_string((int)kArrestedHours) + " hours.";
        result.text = text;
    }

    state.PostEvent(result.success ? GameEventKind::Good : GameEventKind::Bad, result.title + ". " + result.text);
    results.push_back(std::move(result));
    if (results.size() > 8)
        results.erase(results.begin());
}

void CitySim::TickOut(GameState& state, const SimStep& step)
{
    for (CrewOut& entry : out)
    {
        entry.minutesLeft -= step.gameMinutes;
        if (entry.minutesLeft > 0.0f)
            continue;
        if (CrewMember* member = state.FindCrew(entry.crewId))
        {
            ComeBack(state, *member);
            state.PostEvent(GameEventKind::Info, member->name + (entry.arrested ? " made bail and is back." : " is patched up and back."));
        }
    }
    std::erase_if(out, [](const CrewOut& entry) { return entry.minutesLeft <= 0.0f; });
}

void CitySim::TickHeat(GameState& state, const SimStep& step)
{
    m_heatMinutes += step.gameMinutes;
    while (m_heatMinutes >= 60.0f)
    {
        m_heatMinutes -= 60.0f;
        state.AddHeat(CountMenOnMissions() > 0 ? -kHeatDecayPerHourMenOut : -kHeatDecayPerHour);
    }

    const int band = GetHeatBand(state.heat);
    if (band != m_heatBand)
    {
        char text[128];
        if (band > m_heatBand)
        {
            snprintf(text, sizeof(text), "Heat is rising: %s. Missions are %d%% less likely to succeed.", GetHeatBandName(state.heat), -GetHeatChancePenalty(state.heat));
            state.PostEvent(GameEventKind::Bad, text);
        }
        else
        {
            snprintf(text, sizeof(text), "Heat is cooling: %s.", GetHeatBandName(state.heat));
            state.PostEvent(GameEventKind::Info, text);
        }
        m_heatBand = band;
    }
}

bool CitySim::IsAvailable(const GameState& state, const CrewMember& member) const
{
    if (member.role != CrewRole::Soldier || member.away || !IsFitForWork(state, member))
        return false;
    return FindMissionOf(member.id) == nullptr && FindOut(member.id) == nullptr;
}

CrewStatus CitySim::GetCrewStatus(const CrewMember& member) const
{
    if (member.role == CrewRole::Boss)
        return CrewStatus::Boss;
    if (const CrewOut* entry = FindOut(member.id))
        return entry->arrested ? CrewStatus::Arrested : CrewStatus::Wounded;
    if (FindMissionOf(member.id))
        return CrewStatus::OnMission;
    if (member.jobId == kRestJobId)
        return member.restBed != 0 ? CrewStatus::Resting : CrewStatus::Tired;
    return member.jobId != 0 ? CrewStatus::Working : CrewStatus::Free;
}

const ActiveMission* CitySim::FindMissionOf(uint32_t crewId) const
{
    for (const ActiveMission& mission : missions)
        if (std::find(mission.crew.begin(), mission.crew.end(), crewId) != mission.crew.end())
            return &mission;
    return nullptr;
}

const CrewOut* CitySim::FindOut(uint32_t crewId) const
{
    for (const CrewOut& entry : out)
        if (entry.crewId == crewId)
            return &entry;
    return nullptr;
}

const CityMarker* CitySim::FindMarker(uint32_t markerId) const
{
    for (const CityMarker& marker : markers)
        if (marker.id == markerId)
            return &marker;
    return nullptr;
}

MissionChance CitySim::ComputeChance(const GameState& state, const MissionDef& def, const std::vector<uint32_t>& men) const
{
    MissionChance chance;
    chance.base = def.baseChance;
    chance.heat = GetHeatChancePenalty(state.heat);

    int sum = 0;
    int count = 0;
    for (uint32_t id : men)
    {
        for (const CrewMember& member : state.crew)
        {
            if (member.id == id)
            {
                sum += GetCrewStat(member, def.stat);
                ++count;
            }
        }
    }
    if (count > 0)
    {
        chance.stat     = (int)std::lround((float)kChancePerStatPoint * ((float)sum / (float)count - (float)kAverageStat));
        chance.extraMen = kChancePerExtraMan * (count - 1);
    }
    chance.total = std::clamp(chance.base + chance.stat + chance.extraMen + chance.heat, kMinChance, kMaxChance);
    return chance;
}

int CitySim::WoundChance(int nerve)
{
    return std::max(0, kWoundChanceBase - kWoundChancePerNerve * nerve);
}

int CitySim::ArrestChance(int nerve)
{
    return std::max(0, kArrestChanceBase - kArrestChancePerNerve * nerve);
}

std::vector<uint32_t> CitySim::DefaultPick(const GameState& state, const MissionDef& def) const
{
    std::vector<const CrewMember*> candidates;
    for (const CrewMember& member : state.crew)
        if (IsAvailable(state, member))
            candidates.push_back(&member);
    // Free men before working ones, then the best at what's tested.
    std::stable_sort(candidates.begin(), candidates.end(), [&def](const CrewMember* a, const CrewMember* b) {
        if ((a->jobId != 0) != (b->jobId != 0))
            return a->jobId == 0;
        return GetCrewStat(*a, def.stat) > GetCrewStat(*b, def.stat);
    });

    std::vector<uint32_t> men;
    for (const CrewMember* member : candidates)
    {
        if ((int)men.size() >= def.minCrew)
            break;
        men.push_back(member->id);
    }
    return men;
}

const char* CitySim::WhyNotGo(const GameState& state, const MissionDef& def, const std::vector<uint32_t>& men) const
{
    if ((int)men.size() < def.minCrew)
        return def.minCrew == 1 ? "Pick a man" : "Not enough men picked";
    if ((int)men.size() > def.maxCrew)
        return "Too many men picked";
    for (uint32_t id : men)
    {
        bool available = false;
        for (const CrewMember& member : state.crew)
            if (member.id == id)
                available = IsAvailable(state, member);
        if (!available)
            return "Someone picked isn't available";
    }
    if (state.money < def.investment)
        return "Not enough money";
    return nullptr;
}

const char* CitySim::WhyNotHire(const GameState& state, const MissionDef& def) const
{
    if ((int)state.crew.size() >= CrewCapacity(state))
        return "The crew is full";
    if (state.money < def.investment)
        return "Not enough money";
    return nullptr;
}

bool CitySim::StartMission(GameState& state, uint32_t markerId, const std::vector<uint32_t>& men)
{
    const CityMarker* marker = FindMarker(markerId);
    if (!marker)
        return false;
    const MissionDef& def = GetMissionDefs()[(size_t)marker->def];
    if (def.effect != MissionEffect::Job || WhyNotGo(state, def, men))
        return false;

    state.money -= def.investment;
    moneySpent += def.investment;
    ++missionsStarted;

    ActiveMission mission;
    mission.id          = m_nextMissionId++;
    mission.def         = marker->def;
    mission.site        = marker->site;
    mission.crew        = men;
    mission.chance      = ComputeChance(state, def, men).total;
    mission.route       = MakeRoute(kCitySites[marker->site]);
    mission.routeLength = RouteLength(mission.route);

    TileCoord outside = GetWarehouseCenter();
    TileCoord inside  = outside;
    FindWarehouseDoor(state.world, outside, inside);
    for (uint32_t id : men)
    {
        CrewMember* member = state.FindCrew(id);
        DropJob(*member);
        SendCharacterTo(member->motion, state.world, outside); // if there's no way, Leaving lets him go
    }

    state.PostEvent(GameEventKind::Info, JoinNames(state, men) + (men.size() > 1 ? " head out: " : " heads out: ") + def.name + " at " + kCitySites[marker->site].name + ".");
    std::erase_if(markers, [markerId](const CityMarker& m) { return m.id == markerId; });
    missions.push_back(std::move(mission));
    return true;
}

bool CitySim::HireCandidate(GameState& state, uint32_t markerId)
{
    const CityMarker* marker = FindMarker(markerId);
    if (!marker)
        return false;
    const MissionDef& def = GetMissionDefs()[(size_t)marker->def];
    if (def.effect != MissionEffect::Hire || WhyNotHire(state, def))
        return false;

    state.money -= def.investment;
    moneySpent += def.investment;

    CrewMember member;
    member.id     = state.nextCharacterId++;
    member.name   = marker->candidate.name;
    member.role   = CrewRole::Soldier;
    member.tint   = marker->candidate.tint;
    member.muscle = marker->candidate.muscle;
    member.brains = marker->candidate.brains;
    member.nerve  = marker->candidate.nerve;
    ComeBack(state, member);
    state.PostEvent(GameEventKind::Good, member.name + " joins the family.");
    state.crew.push_back(std::move(member));

    std::erase_if(markers, [markerId](const CityMarker& m) { return m.id == markerId; });
    return true;
}

int CitySim::CountMenOnMissions() const
{
    int count = 0;
    for (const ActiveMission& mission : missions)
        count += (int)mission.crew.size();
    return count;
}

int CitySim::CountArrested() const
{
    int count = 0;
    for (const CrewOut& entry : out)
        count += entry.arrested ? 1 : 0;
    return count;
}
