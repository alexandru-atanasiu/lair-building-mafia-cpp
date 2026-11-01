#pragma once

#if defined(GAME_DEV_TOOLS_ENABLED)

#include <sstream>
#include <string>

struct GameState;

// The dev driver's "city ..." commands. Returns the reply ("ok ..." or "err ...").
//   city markers                 id:key:site:screenX:screenY:hoursLeft for each marker
//   city spawn <key>             a new marker of that mission (shakedown, burglary, ...)
//   city go <marker> <crew ids>  Go, without the popup
//   city hire <marker>           Hire, without the popup
//   city crew                    name:status:muscle:brains:nerve:jobId:away for each man
//   city missions                id:key:phase:crew:chance:car position for each one going on
//   city job <crew id> <jobId>   gives a man a job number, to test taking men off their work
//   city totals                  started, won, lost, earned, spent, heat from missions
//   city auto <0..5>             nobody, greedy missions, careful missions, smuggling up to booze,
//                                smuggling with missions by free men while heat is under 50,
//                                smuggling with contraband
//                                Every one of them builds bunks when its men start to tire.
std::string ExecuteCityCommand(GameState& state, std::istringstream& in);

void RunCityAutoPlayer(GameState& state, int mode);

#endif
