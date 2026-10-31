#include "sim_cheats.h"

#if defined(GAME_IMGUI_ENABLED)

#include "../sim/simulation.h"
#include "debug/debug_pad.h"

#include <imgui.h>

#include <cstdio>

namespace
{
    constexpr const char* kCategory = "Simulation";
}

void RegisterSimCheats(DebugPad& pad, Simulation& simulation)
{
    pad.RegisterCategory(kCategory, [&simulation]() {
        GameState& state = simulation.GetState();

        int money = (int)state.money;
        if (ImGui::InputInt("Money", &money, 100, 1000))
            state.money = money;
        ImGui::SliderFloat("Heat", &state.heat, 0.0f, 100.0f, "%.0f");

        ImGui::Text("Day %d  %02d:%02d", state.clock.GetDay(), state.clock.GetHour(), state.clock.GetMinute());
        bool paused = state.clock.IsPaused();
        if (ImGui::Checkbox("Paused", &paused))
            state.clock.SetPaused(paused);
        for (int speed = 1; speed <= GameClock::kMaxSpeed; ++speed)
        {
            ImGui::SameLine();
            char label[8];
            snprintf(label, sizeof(label), "%dx", speed);
            if (ImGui::RadioButton(label, state.clock.GetSpeed() == speed))
                state.clock.SetSpeed(speed);
        }

        ImGui::Checkbox("Idle crew wander", &state.lair.wanderEnabled);
        if (ImGui::Button("Post test event"))
            state.PostEvent(GameEventKind::Info, "A test event from the DebugPad.");

        for (const CrewMember& member : state.crew)
        {
            const TileCoord tile = state.world.WorldToTile(member.motion.level, member.motion.position);
            ImGui::Text("%-8s %-7s %s (%d, %d) %s", member.name.c_str(), member.role == CrewRole::Boss ? "boss" : "soldier",
                        member.motion.level == WorldLevel::Ground ? "ground" : "under", tile.x, tile.z,
                        member.motion.IsMoving() ? "walking" : "idle");
        }
    });
}

void UnregisterSimCheats(DebugPad& pad)
{
    pad.UnregisterCategory(kCategory);
}

#endif
