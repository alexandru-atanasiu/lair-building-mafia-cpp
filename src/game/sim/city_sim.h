#pragma once

#include "city_map_data.h"
#include "core/color.h"
#include "core/math.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

struct CrewMember;
struct GameState;
struct SimStep;

// The city map: crew sent out one by one on missions, which earn money and make heat. No drawing
// or input here: CityScene (scenes/city_scene.*) shows this and calls the functions below.
//
// What the law can read: GameState::heat (0..100, kept by AddHeat), GetHeatChancePenalty,
// CitySim::CountMenOnMissions, CountArrested, GetCrewStatus. Heat falls here, once a game hour.

enum class CrewStat
{
    Muscle,
    Brains,
    Nerve,
};

enum class MissionEffect
{
    Job,  // men go, money and heat come back
    Hire, // nobody goes: pay, and a new man turns up
};

// One row of the mission table (kMissions in city_sim.cpp). Adding a mission is adding a row.
struct MissionDef
{
    const char*   key;        // for dev commands
    const char*   name;
    const char*   blurb;
    const char*   icon;       // cooked texture
    Color         color;      // of its marker
    CityLocation  location;   // the kind of place its markers appear at
    MissionEffect effect;
    CrewStat      stat;       // tested by the chance of success
    int           minCrew;
    int           maxCrew;
    float         hours;      // on the job, not counting the drive
    int           baseChance; // % for men with an average stat of kAverageStat, before heat
    int           investment; // charged by Go, lost on failure
    int           reward;     // paid on success
    float         heat;       // added on success (negative: lowers it)
    float         failHeat;   // added on failure
    float         weight;     // how often its markers appear, relative to the others
    float         markerHours; // how long a marker waits before it expires
    int           maxLive;    // markers of it at once
    float         evidence = 0.0f; // added to the law's evidence on success (negative: lowers it)
};
std::span<const MissionDef> GetMissionDefs();

// A man offered by a Hire marker.
struct RecruitCandidate
{
    std::string name;
    int         muscle = 1;
    int         brains = 1;
    int         nerve  = 1;
    Color       tint;
};

struct CityMarker
{
    uint32_t         id = 0;
    int              def = 0;  // into GetMissionDefs()
    int              site = 0; // into kCitySites
    double           expiresAt = 0.0; // game minutes
    float            lifeMinutes = 1.0f;
    RecruitCandidate candidate; // Hire only
};

enum class MissionPhase
{
    Leaving,     // the men walk to the warehouse's door
    DrivingOut,
    Working,
    DrivingBack,
};

struct ActiveMission
{
    uint32_t              id = 0;
    int                   def = 0;
    int                   site = 0;
    std::vector<uint32_t> crew;   // still on it: the wounded and arrested drop out when it's decided
    int                   chance = 0; // fixed by Go
    MissionPhase          phase = MissionPhase::Leaving;
    std::vector<Vec2>     route;  // along the roads, warehouse to site, in map pixels
    float                 routeLength = 0.0f;
    float                 routePos = 0.0f; // pixels along the route
    float                 workMinutesLeft = 0.0f;

    // Where the car is, and which way it's heading.
    void SampleCar(Vec2& position, Vec2& direction) const;
};

// Someone who can't come back yet.
struct CrewOut
{
    uint32_t crewId = 0;
    bool     arrested = false; // or wounded
    float    minutesLeft = 0.0f;
};

enum class CrewStatus
{
    Boss,
    Free,
    Working,
    OnMission,
    Wounded,
    Arrested,
};

// A mission decided: the scene shows these as cards; they also go to the news.
struct MissionResult
{
    bool        success = false;
    std::string title;
    std::string text;
};

// Pieces of a chance of success, in percentage points, as shown by the popup.
struct MissionChance
{
    int base = 0;
    int stat = 0;
    int extraMen = 0;
    int heat = 0;  // zero or negative
    int total = 0; // clamped
};

// 0, -10, -20, -30: what heat does to every mission's chance. The bands are for the player; the
// law's own reactions to heat are in law_sim.
int         GetHeatChancePenalty(float heat);
int         GetHeatBand(float heat); // 0..3
const char* GetHeatBandName(float heat);
int         GetCrewStat(const CrewMember& member, CrewStat stat);
const char* GetCrewStatName(CrewStat stat);

struct CitySim
{
    static constexpr int kMissionJobId = 0x0C17; // CrewMember::jobId while a man is walking out

    // Stats for the starting crew, first markers. Called once by GameState's constructor.
    void Init(GameState& state);
    void Tick(GameState& state, const SimStep& step);

    // A man who can be picked: a soldier here, not hurt or locked up. He may be working.
    bool       IsAvailable(const GameState& state, const CrewMember& member) const;
    CrewStatus GetCrewStatus(const CrewMember& member) const;
    const ActiveMission* FindMissionOf(uint32_t crewId) const;
    const CrewOut*       FindOut(uint32_t crewId) const;
    const CityMarker*    FindMarker(uint32_t markerId) const;

    MissionChance ComputeChance(const GameState& state, const MissionDef& def, const std::vector<uint32_t>& men) const;
    // Odds, in %, that a man with this Nerve is wounded or arrested if a mission fails.
    static int WoundChance(int nerve);
    static int ArrestChance(int nerve);

    // Who the popup picks first: the best free men, then the best working ones, up to minCrew.
    std::vector<uint32_t> DefaultPick(const GameState& state, const MissionDef& def) const;
    // Why Go can't be pressed; null if it can.
    const char* WhyNotGo(const GameState& state, const MissionDef& def, const std::vector<uint32_t>& men) const;
    const char* WhyNotHire(const GameState& state, const MissionDef& def) const;

    // Charges the investment and sends the men. False (nothing changes) if WhyNotGo says so.
    bool StartMission(GameState& state, uint32_t markerId, const std::vector<uint32_t>& men);
    bool HireCandidate(GameState& state, uint32_t markerId);

    // A new marker of this mission, on a free site. False if it has no free site.
    bool SpawnMarker(GameState& state, int def);

    int CountMenOnMissions() const;
    int CountArrested() const;

    std::vector<CityMarker>    markers;
    std::vector<ActiveMission> missions;
    std::vector<CrewOut>       out;
    std::vector<MissionResult> results; // oldest first; the scene takes them off

    // Totals, for tuning and for anyone curious.
    int     missionsStarted = 0;
    int     missionsWon = 0;
    int     missionsLost = 0;
    int64_t moneyEarned = 0;
    int64_t moneySpent = 0;
    float   heatFromMissions = 0.0f;

    // Dev tool: 0 off, 1 runs everything it can (greedy), 2 a careful player. See dev/city_dev.cpp.
    int devAutoPlayer = 0;

    // Where the scene last showed the map, physical pixels: screen = offset + map * scale.
    float screenScale = 1.0f;
    float screenOffsetX = 0.0f;
    float screenOffsetY = 0.0f;

private:
    void TickMarkers(GameState& state, const SimStep& step);
    void TickMissions(GameState& state, const SimStep& step);
    void TickOut(GameState& state, const SimStep& step);
    void TickHeat(GameState& state, const SimStep& step);
    void Resolve(GameState& state, ActiveMission& mission);
    bool SiteIsFree(int site) const;
    RecruitCandidate MakeCandidate(GameState& state) const;

    uint32_t m_nextMarkerId  = 1;
    uint32_t m_nextMissionId = 1;
    float    m_spawnMinutes  = 0.0f;
    float    m_heatMinutes   = 0.0f;
    int      m_heatBand      = 0;
};
