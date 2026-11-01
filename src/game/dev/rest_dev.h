#pragma once

#if defined(GAME_DEV_TOOLS_ENABLED)

#include <sstream>
#include <string>

struct GameState;

// Reading and setting tiredness, for the dev driver:
//   rest                         beds=.. free=.. off=.. bed<id>=x:z:<crew id, 0 if free> for each
//                                bed, man<id>=name:energy:status:bed for each man (status: boss,
//                                free, working, mission, wounded, arrested, tired, tobed, resting)
//   rest set <crew id|all> <n>   sets the meter, 0..100
std::string ExecuteRestCommand(GameState& state, std::istringstream& in);

#endif
