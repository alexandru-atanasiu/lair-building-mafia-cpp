#pragma once

#include "../character/character_motion.h"
#include "../world/world_grid.h"
#include "city_sim.h"
#include "core/color.h"
#include "lair_sim.h"
#include "law_sim.h"
#include "warehouse_sim.h"

#include <cstdint>
#include <deque>
#include <random>
#include <string>
#include <vector>

// The game itself, shared by every view and owned outside them (by Game, through Simulation).
// Nothing under src/game/sim/ draws or reads input. Views read and change this state directly.
//
// Where each later system goes: its data and Tick in its own pair of files, already hooked up:
//   warehouse_sim.h  the truck and the warehouse
//   lair_sim.h       digging, rooms, objects, crew jobs
//   city_sim.h       missions
//   law_sim.h        heat over time, investigators, raids, escapes

// One step of the clock: both are 0 while paused.
struct SimStep
{
    float simSeconds  = 0.0f; // real seconds x game speed: what walking and animation follow
    float gameMinutes = 0.0f; // minutes on the game's day clock
};

// Day clock with pause and speeds, as in Evil Genius: a game day is 2 real minutes at 1x.
class GameClock
{
public:
    static constexpr int   kMaxSpeed            = 3;
    static constexpr float kGameMinutesPerSimSecond = 12.0f;

    SimStep Advance(float realSeconds);

    bool IsPaused() const { return m_paused; }
    void SetPaused(bool paused) { m_paused = paused; }
    int  GetSpeed() const { return m_speed; }
    void SetSpeed(int speed); // 1..kMaxSpeed

    double GetTotalMinutes() const { return m_minutes; }
    int    GetDay() const { return 1 + (int)(m_minutes / (24.0 * 60.0)); } // counts from 1
    int    GetHour() const { return (int)(m_minutes / 60.0) % 24; }
    int    GetMinute() const { return (int)m_minutes % 60; }

private:
    double m_minutes = 8.0 * 60.0; // starts on day 1 at 08:00
    int    m_speed   = 1;
    bool   m_paused  = false;
};

enum class GameEventKind
{
    Info,
    Good,
    Bad,
};

// A line of news for the player. Views read GameState::events; the HUD shows the newest.
struct GameEvent
{
    uint64_t      id = 0; // increasing
    GameEventKind kind = GameEventKind::Info;
    std::string   text;
    double        atMinutes = 0.0;
};

enum class CrewRole
{
    Soldier,
    Boss, // the one the family can't lose; stands out by his color
};

struct CrewMember
{
    uint32_t        id = 0; // unique among characters: the key for CharacterView
    std::string     name;
    CrewRole        role = CrewRole::Soldier;
    Color           tint; // the body's color: every character is the same model, see CharacterView
    CharacterMotion motion;
    bool            away = false;     // out on a mission: not in the place, not drawn
    int             muscle = 2;       // 1..5, see CitySim
    int             brains = 2;
    int             nerve = 2;
    float           idleTime = 0.0f;  // sim seconds left before wandering on (lair_sim.cpp)
    int             jobId = 0;        // 0 = free for work; whoever gives crew work sets their own value, and clears it when done
};

struct GameState
{
    GameState();

    // Clamped to 0..100.
    void AddHeat(float amount);
    // Adds to the news; old lines fall off the end.
    void PostEvent(GameEventKind kind, std::string text);
    CrewMember* FindCrew(uint32_t id);

    int64_t   money = 5000;
    float     heat  = 0.0f; // 0..100
    GameClock clock;

    WorldGrid               world;
    std::vector<CrewMember> crew;
    std::deque<GameEvent>   events; // oldest first
    uint64_t                nextEventId = 1;
    uint32_t                nextCharacterId = 1; // for anything drawn through CharacterView
    int                     crates = 0; // smuggled crates in the warehouse: WarehouseSim::stored says where
    std::minstd_rand        random; // fixed seed: runs repeat

    WarehouseSim warehouse;
    LairSim      lair;
    CitySim      city;
    LawSim       law;
};
