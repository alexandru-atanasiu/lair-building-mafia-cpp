#pragma once

#if defined(GAME_IMGUI_ENABLED)

class DebugPad;
class Simulation;

// "Simulation" DebugPad category: money, heat, the clock, news, crew.
// `simulation` must outlive the registration.
void RegisterSimCheats(DebugPad& pad, Simulation& simulation);
void UnregisterSimCheats(DebugPad& pad);

#endif
