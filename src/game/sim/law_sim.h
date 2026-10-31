#pragma once

#include "../character/character_motion.h"
#include "core/color.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct GameState;
struct SimStep;

// The law. As heat rises investigators walk in from the road, look around and report what they saw
// as evidence. When heat and evidence are both high, a warning, then a raid played out in the world:
// the family gets away twice, moving to new premises, and the third raid catches the boss.
//
// GameState holds one of these and Simulation::Advance calls Tick on every step of the clock. While
// a screen is up (LawScreen) the simulation doesn't advance at all, so nothing can happen behind it.

// ---- Tuning: a game hour is 5 sim seconds at 1x, a game day 120. ----

// The raid: both of these at once.
constexpr float kRaidHeat     = 60.0f;
constexpr float kRaidEvidence = 60.0f;
constexpr int   kLawEscapes   = 2;      // raids the boss gets away from; the next one catches him

constexpr float kEvidenceMax           = 100.0f;
constexpr float kEvidenceDecayHeat     = 40.0f;  // evidence only fades below this heat
constexpr float kEvidenceDecayPerHour  = 0.25f;

// Investigators, by heat band (GetHeatBand: 0 quiet, 1 noticed, 2 hot, 3 scorching). A visit takes
// about 12-15 game hours, door to door.
constexpr float kVisitEveryHours[4]  = { 0.0f, 30.0f, 16.0f, 10.0f }; // 0 = nobody comes
constexpr int   kVisitorsAtOnce[4]   = { 0, 1, 1, 3 };
constexpr float kVisitJitter         = 0.3f;  // +- this fraction of the interval
constexpr float kFirstVisitFraction  = 0.4f;  // of the interval, after heat enters a band
constexpr float kLookSeconds         = 2.5f;  // sim seconds at each place he stops
constexpr float kSightRadius         = 10.0f; // metres
constexpr int   kLairStopsPerVisit   = 3;
constexpr float kMaxVisitSeconds     = 130.0f; // sim seconds; after this he slips off and reports

// What a report adds to the evidence.
constexpr float kEvidencePerCrate[3]   = { 0.5f, 1.0f, 3.0f }; // by shipment kind: cigarettes, booze, contraband
constexpr float kShelvedCrateFactor    = 0.25f; // a crate that fits on the Storage shelves, worst kind first
constexpr float kGunRackEvidence       = 6.0f;  // used by the object table, lair_tables.cpp
constexpr float kWorkbenchEvidence     = 2.0f;
constexpr float kEvidencePerLooseGrand = 1.0f;  // per $1,000 outside the vault, if he gets down to the lair
constexpr float kLooseMoneyEvidenceMax = 10.0f;
constexpr float kIdleManEvidence       = 0.5f;  // a soldier loafing, with no job and no working room to be in
constexpr float kCleanReportEvidence   = -3.0f; // he found nothing to speak of (under kCleanReportBelow)
constexpr float kCleanReportBelow      = 2.0f;

// Crates that draw attention just by sitting there: heat per game hour, per crate not on a shelf.
constexpr float kStoredHeatPerHour[3] = { 0.0f, 0.0f, 0.1f };

// The raid.
constexpr float kRaidWarningHours  = 6.0f;  // from the warning to the police turning up: 30 real seconds at 1x
constexpr int   kRaidOfficers      = 6;
constexpr float kOfficerSpeed      = 4.2f;  // metres per sim second
constexpr float kBossFleeSpeed     = 4.6f;
constexpr float kRaidEndSeconds    = 3.0f;  // sim seconds after the last officer is in place
constexpr float kRaidMaxSeconds    = 60.0f; // the raid ends by then, whatever is in anyone's way

// What a raid costs.
constexpr float kRaidLooseMoneyLoss = 0.30f;
constexpr float kRaidVaultMoneyLoss = 0.15f; // of what's safe in the vault (ProtectedMoney)
constexpr float kRaidObjectLoss     = 0.5f;
constexpr int   kRaidArrestChance   = 50;    // %, for each man still in the building
constexpr float kRaidArrestHours    = 24.0f;

// A crew member's jobId while the law decides what he does: the boss, running for it or caught.
constexpr int kLawJobId = 1002;

// ---- State ----

enum class LawAgentKind
{
    Investigator,
    Officer,
};

struct LawStop
{
    TileCoord tile;
    float     lookSeconds = 0.0f; // stays this long once there
};

// Someone of theirs walking the world. Not part of GameState::crew: nothing that gives crew work
// sees them. CharacterView draws them through LevelView, under `id`.
struct LawAgent
{
    uint32_t             id = 0;
    LawAgentKind         kind = LawAgentKind::Investigator;
    std::string          name;
    Color                tint;
    CharacterMotion      motion;
    std::vector<LawStop> route;
    size_t               nextStop = 0;
    float                lookSeconds = 0.0f;
    float                pendingLook = 0.0f;
    float                totalSeconds = 0.0f;
    float                sightTimer = 0.0f;
    bool                 finished = false; // an officer in place; an investigator goes as soon as he's done

    // What an investigator has seen. Only counts once he leaves and reports.
    std::vector<std::pair<int, int>> seenCrates;  // slot, kind
    std::vector<int>                 seenObjects; // LairObject ids
    std::vector<uint32_t>            seenMen;
    bool                             wentBelow = false;
    int64_t                          looseMoneySeen = 0;
};

enum class RaidPhase
{
    None,
    Warning, // counting down
    Police,  // officers on the way in or inside
};

// A screen that stops the game until the player has read it.
enum class LawScreen
{
    None,
    Escaped,
    Busted,
};

struct LawReport
{
    std::string              title;
    std::vector<std::string> lines;
};

struct LawSim
{
    void Tick(GameState& state, const SimStep& step);

    int  EscapesLeft() const { return raidsSurvived < kLawEscapes ? kLawEscapes - raidsSurvived : 0; }
    bool IsFinalRaid() const { return raidsSurvived >= kLawEscapes; }
    void AddEvidence(float amount);

    int CountAgents(LawAgentKind kind) const;

    // Starts walking now, as if their time had come.
    void SpawnInvestigators(GameState& state, int count);
    // The warning, then the police. `noWarning`: straight to the police.
    void StartRaid(GameState& state, bool noWarning);
    // The player has read the screen: the game goes on. Bust: asks Game for a new game.
    void DismissScreen(GameState& state);

    float evidence = 0.0f; // 0..kEvidenceMax

    std::vector<LawAgent> agents;
    std::vector<uint32_t> departedIds; // left the world: Game frees their animation state

    RaidPhase phase = RaidPhase::None;
    float     warningMinutes = 0.0f;  // Warning: game minutes left
    float     raidSeconds = 0.0f;     // Police: sim seconds since they appeared
    float     endSeconds = 0.0f;      // Police: since everyone was in place
    bool      policeInside = false;
    bool      bossRunning = false;    // Police: on the way to the bolt-hole
    int       speedBeforeRaid = 0;    // the warning drops to 1x; back to this after the screen. 0 = none

    int raidsSurvived = 0;

    LawScreen screen = LawScreen::None;
    LawReport report;
    bool      newGameWanted = false;  // Game acts on it

    // Totals, for the last screen.
    int investigatorsSeen = 0;
    int reportsFiled = 0;

    // Where the screen's button is, physical pixels: for the dev driver.
    int screenButtonX = 0;
    int screenButtonY = 0;

    // Dev tools. See dev/law_dev.cpp.
    int  devTurbo = 1;           // simulation steps per frame
    bool devAutoDismiss = false; // escape screens go away by themselves, for auto players
    std::vector<std::string> devLog;

private:
    void TickSlowChanges(GameState& state, const SimStep& step);
    void TickVisits(GameState& state, const SimStep& step);
    void TickAgents(GameState& state, const SimStep& step);
    void TickRaid(GameState& state, const SimStep& step);
    void SendInOfficers(GameState& state);
    void BossReacts(GameState& state);
    void MoveHouse(GameState& state);
    void BustBoss(GameState& state);
    void ClearAgents();

    float m_visitMinutes = -1.0f; // until the next visit; < 0 = not counting
    int   m_visitBand = 0;
    int   m_nextName = 0;
};
