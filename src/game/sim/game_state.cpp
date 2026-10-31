#include "game_state.h"

#include "../world/world_layout.h"
#include "core/assert.h"

#include <algorithm>

namespace
{
    constexpr size_t kMaxEvents = 64;

    struct StartingCrew
    {
        const char* name;
        CrewRole    role;
        Color       tint;
        TileCoord   tile;
    };
    const StartingCrew kStartingCrew[] = {
        // The boss is the only saturated color; the soldiers wear dull suits, none of them red.
        { "Don Carlo", CrewRole::Boss,    { 0.95f, 0.08f, 0.06f, 1.0f }, { WorldLevel::Underground, 19, 24 } },
        { "Sal",       CrewRole::Soldier, { 0.20f, 0.23f, 0.32f, 1.0f }, { WorldLevel::Underground, 19, 19 } },
        { "Vinnie",    CrewRole::Soldier, { 0.34f, 0.27f, 0.20f, 1.0f }, { WorldLevel::Underground, 22, 20 } },
        { "Paulie",    CrewRole::Soldier, { 0.46f, 0.46f, 0.48f, 1.0f }, { WorldLevel::Underground, 18, 25 } },
        { "Tony",      CrewRole::Soldier, { 0.22f, 0.30f, 0.22f, 1.0f }, { WorldLevel::Underground, 23, 26 } },
        { "Frankie",   CrewRole::Soldier, { 0.18f, 0.32f, 0.36f, 1.0f }, { WorldLevel::Ground, 17, 14 } },
    };
}

SimStep GameClock::Advance(float realSeconds)
{
    SimStep step;
    if (m_paused)
        return step;
    step.simSeconds  = realSeconds * (float)m_speed;
    step.gameMinutes = step.simSeconds * kGameMinutesPerSimSecond;
    m_minutes += step.gameMinutes;
    return step;
}

void GameClock::SetSpeed(int speed)
{
    GAME_ASSERT(speed >= 1 && speed <= kMaxSpeed, "Invalid game speed %d", speed);
    m_speed = std::clamp(speed, 1, kMaxSpeed);
}

GameState::GameState()
    : world(MakeStartingWorld())
{
    for (const StartingCrew& start : kStartingCrew)
    {
        CrewMember member;
        member.id   = nextCharacterId++;
        member.name = start.name;
        member.role = start.role;
        member.tint = start.tint;
        PlaceCharacter(member.motion, world, start.tile);
        crew.push_back(std::move(member));
    }
}

void GameState::AddHeat(float amount)
{
    heat = std::clamp(heat + amount, 0.0f, 100.0f);
}

void GameState::PostEvent(GameEventKind kind, std::string text)
{
    GameEvent event;
    event.id        = nextEventId++;
    event.kind      = kind;
    event.text      = std::move(text);
    event.atMinutes = clock.GetTotalMinutes();
    events.push_back(std::move(event));
    if (events.size() > kMaxEvents)
        events.pop_front();
}

CrewMember* GameState::FindCrew(uint32_t id)
{
    for (CrewMember& member : crew)
        if (member.id == id)
            return &member;
    return nullptr;
}
