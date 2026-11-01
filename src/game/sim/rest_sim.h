#pragma once

#include "../world/world_grid.h"

struct CrewMember;
struct GameState;
struct SimStep;

// Tiredness: every soldier has an energy meter, and bunks are where he fills it. Going off work is
// one more CrewMember::jobId: kRestJobId overwrites whatever he was doing, and that job's owner
// sees it isn't theirs any more and puts the work back, as for a man sent on a mission.
//
// The boss has a meter too but it stays full: he takes no jobs, so there's nothing to stop doing.

// ---- Tuning: energy is 0..100, rates are per game hour ----

constexpr float kEnergyFull               = 100.0f;
constexpr float kTiredEnergy              = 20.0f; // below this he goes off work
constexpr float kRestedEnergy             = 90.0f; // a man in a bed gets up at this
constexpr float kFitEnergy                = 40.0f; // a man with no bed goes back at this: not rested, but fit
constexpr float kEnergyLossWorkingPerHour = 4.0f;  // full to tired in 20 hours: about a day's shift
constexpr float kEnergyLossIdlePerHour    = 1.5f;  // full to tired in about 53 hours
constexpr float kEnergyGainInBedPerHour   = 20.0f; // tired to rested in 3.5 hours
constexpr float kEnergyGainNoBedPerHour   = 2.5f;  // tired to fit in 8 hours, and then only 5 hours of work
constexpr float kNoBedNewsHours           = 6.0f;  // at most one line of news in this time about a lack of beds

// A crew member's jobId while he's off work, in a bed, on his way to one or waiting for one.
constexpr int kRestJobId = 1003;

struct RestSim
{
    double lastNoBedNews = -1e9; // game minutes
    float  bedTimer      = 0.0f;

    void Tick(GameState& state, const SimStep& step);
};

// ---- Who may work (every system that picks a man asks this) ----

// Not off work and not worn out. In a raid everyone counts: the family's response comes first.
bool IsFitForWork(const GameState& state, const CrewMember& member);

// ---- Reading ----

int  CountBeds(const GameState& state);     // beds in working rooms
int  CountFreeBeds(const GameState& state); // of those, the ones nobody has
int  CountMenInBed(const GameState& state);
int  CountMenOffWork(const GameState& state); // in bed, on the way, or waiting for one
