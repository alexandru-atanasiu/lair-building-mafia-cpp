#pragma once

#if defined(GAME_DEV_TOOLS_ENABLED)

#include <sstream>
#include <string>

struct GameState;

// The dev driver's "law" commands; the reply is "ok ..." or "err ...".
//   law                     evidence, raid phase, escapes left, the agents, the screen
//   law evidence <n>        set the evidence
//   law investigator [n]    n investigators start walking now
//   law raid                the warning starts now
//   law raidnow             the police start walking now, with no warning
//   law third               as `raid`, as if this were the third raid
//   law dismiss             what the screen's button does
//   law report              the screen's text
//   law autodismiss <0|1>   escape screens go away by themselves, for auto players
//   law turbo <n>           n simulation steps per frame, for tuning
//   law check               "ok" if no man is stuck or owned twice, otherwise what's wrong
//   law log                 reports and raids so far, for tuning
std::string ExecuteLawCommand(GameState& state, std::istringstream& in);

#endif
