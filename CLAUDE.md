# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

"Capo di Tutti Capi" (working title): an Evil Genius style lair builder about a mob family, in real 3D. Windows-only C++20 / Direct3D 11, with skeletal animation (ozz-animation, GPU skinning), instanced 3D meshes with Gouraud lighting, instanced 2D sprites, game UI with baked bitmap fonts, and developer tooling built on Dear ImGui. No CMake/MSBuild: everything is driven by batch scripts that call `cl`, `link` and `fxc` directly. The engine parts were ported from an earlier in-house project. Tentative design: `docs/DESIGN.md` (its "As built" section describes an earlier C# version and is only background).

Three views share one simulation and one clock: the **warehouse** (ground level, 3D), the **lair** (underground level, 3D) and the **city** (2D map). They play as one loop: buy shipments and carry them in through the warehouse, sell them on, send men on city missions, and spend the money on the lair, whose bunks and shelves in turn let you hire more men and store more crates. Heat rises from both, and high heat brings the law: investigators who report evidence, then raids; the family escapes twice and the third raid ends the game. See "The loop and the crew" and "The law".

## Commands

The scripts set up the Visual Studio x64 environment themselves when needed (`env.bat`; it finds `vcvars64.bat` at the default VS 18 path, or set `VCVARS64`). Python 3 with Pillow must be on PATH. Blender is found through `BLENDER`, then PATH, then the default 5.2 install folder (`py/cook.py`). Run the scripts by path (`.\build.bat`, or a full path from `cmd /c`): some shells don't search the current directory.

```bat
build_tools.bat          :: asset tools -> __generated\tools (cook.bat runs it only if anim_cook.exe is missing: rerun after changing tools/ or vendor/ozz)
cook.bat [--force] [--debug-shaders]  :: shaders/ + assets/ -> __generated\data (rerun after any shader/asset change; only stale outputs are cooked)
build.bat [dbg|opt|ship] :: default dbg; output in __generated\bin\<cfg>\ (game.exe, game.pdb, obj\)
run.bat   [dbg|opt|ship] [args]  :: runs the exe with __generated\data as the working directory
deploy.bat [cook args]   :: build.bat ship + cook.bat, then exe + data (no pdb, no shader_mode.txt) -> __generated\deploy (self-contained, zip as is)
```

From a clean checkout: `build_tools.bat`, `cook.bat`, `build.bat <cfg>`, `run.bat <cfg>`.

Game args: `-windowed` / `-fullscreen`, `-vsync` / `-novsync` (override per-config defaults; parsed in `src/game/launch_options.cpp`), plus the dev driver's in dbg and opt (below).

There are no tests or linters. Our code is fully rebuilt every time (`build.bat` deletes `obj\*.obj` first); vendor objects (`obj\ozz\`, `obj\imgui\`) are reused while their stamp (compiler flags + the vendor's `VERSION.txt`) is unchanged (delete the folder to force). Verify changes by building **all three configs** (ship compiles out most dev code, so it catches different errors) and running the game: drive it with the dev driver and look at screenshots.

## Dev driver: driving and seeing the game (dbg and opt only)

`src/game/dev/dev_driver.*`, compiled out of ship. It runs commands on the game's thread, one at a time, and its simulated input replaces real input once in use (moving your mouse can't disturb a run; ImGui doesn't take it either).

```bat
python py\dev_client.py --launch dbg --args="-view lair -nodevui" "wait 2" "screenshot shots\lair.png" "key tab" "state" quit
```

`dev_client.py` starts the game with `-pipe capo_dev`, sends each argument as a command and prints the reply (`ok ...` / `err ...`); exit code 1 if one failed. Without `--launch` it talks to a game already running. Other ways in: `run.bat dbg -run "wait 2; screenshot C:\full\path.png; quit"` and `-script <file>` (one command per line, `#` comments).

Launch args: `-view warehouse|lair|city`, `-pipe <name>`, `-run "<a>; <b>"`, `-script <file>`, `-nodevui` (no ImGui: looks like ship), `-size <w>x<h>`, `-dpiscale <scale>` (pretend the monitor's DPI, to test UI scaling).

Commands (pixel coordinates are physical client pixels, top-left origin; `state` gives the window size and DPI scale):

| Command | Does |
|---|---|
| `view warehouse\|lair\|city` | switch view |
| `key <name>`, `keydown <name>`, `keyup <name>` | tap / press / release (`tab space escape shift control 1 2 3 w a s d q e r left right up down`) |
| `move <x> <y>`, `click <x> <y> [left\|right\|middle]`, `mousedown [b]`, `mouseup [b]`, `wheel <notches>` | mouse |
| `tile <ground\|under> <x> <z>` | replies `ok <x> <y>`: where that tile's center is on screen in the current view, on top of what's shown there (in the lair, a block of earth at its current height); use it to click on the world |
| `focus <ground\|under> <x> <z>` | point the current view's camera at a tile |
| `goto <crew id> <ground\|under> <x> <z>` | send a crew member somewhere (`wander 0` first, or they soon wander off) |
| `wander 0\|1` | idle crew wander around, or stay put |
| `money <n>`, `heat <n>`, `speed <1\|2\|3>`, `pause <0\|1>` | simulation |
| `wait <seconds>`, `frames <n>` | wait (real time, frames) before the next command |
| `screenshot <path>` | PNG of the next finished frame. Use an absolute path (`dev_client.py` makes them absolute); the game's working directory is `__generated\data` |
| `devui 0\|1`, `debugpad 0\|1` | hide / show ImGui, close / open the DebugPad |
| `lair` | replies `ok digs=.. builds=.. workers=.. capacity=.. crates=.. protected=.. room1=bunks:x0:z0:x1:z1:working door1=x:z:x\|z:built object1=bunk:x:z:rotation:built ...` |
| `order cigarettes\|booze\|contraband`, `shipout` | the warehouse panel's buttons: they start the truck or join its queue; replies `ok ahead=<n>`, the orders in front |
| `queue`, `queue cancel <n>` | the truck queue, counted from 1: `ok length=2 max=5 ahead=3 q1=booze:8:1:2400 q2=shipout:0:1:0 stored=ccbb news=...` (`name:crates:hours:paid`, each rack crate's kind by first letter, the newest news) |
| `crates <n>`, `crewjob <crew id> <n>` | set the stored crates; set a crew member's `jobId` (non-zero makes them busy) |
| `state` | replies `ok view=Lair money=5000 heat=0 day=1 hour=8 ... crates=0 capacity=24 truck=gone hold=none cargo=0 truckminutes=0 queue=none carriers=Sal*,Tony paymentsdue=0 ... crew1=Don Carlo:boss:under:19:24:idle:0 ...` (`*` = holding a crate; a crew entry ends in the man's `jobId`) |
| `city markers\|crew\|missions\|totals`, `city spawn <key>`, `city go <marker> <crew ids>`, `city hire <marker>`, `city job <crew id> <jobId>`, `city results`, `city auto <0..5>` | the city map (`src/game/dev/city_dev.h`): `markers` gives each one's screen position to click; `auto` plays for tuning: 1 greedy missions, 2 careful missions, 3 smuggling up to booze, 4 smuggling plus missions by free men under heat 50, 5 smuggling with contraband |
| `law`, `law evidence <n>`, `law investigator [n]`, `law raid`, `law raidnow`, `law third`, `law dismiss`, `law report`, `law check`, `law log`, `law autodismiss <0\|1>`, `law turbo <n>` | the law (`src/game/dev/law_dev.h`): `law` gives evidence, raid phase, escapes, agents and the screen's button to click; `raid` starts the warning, `raidnow` skips it, `third` makes it the last one; `check` replies `err ...` if a man is stuck or owned twice, or anyone's standing in a fixture; `turbo` runs n simulation steps per frame, for tuning |
| `city buttons` | where the open popup's main button and the result card's OK are, for clicking |
| `quit` | close the game |

A click right after moving far can miss: `Ui` takes a press only if the mouse was over that widget's panel on the frame before. `move`, `frames 2`, then `click`, as a hand does. Add a command in `DevDriver::Execute`. Ship has no driver: `py/capture_window.py` screenshots a running window from outside and posts clicks and keys to it (`run.bat ship -windowed`, then `python py\capture_window.py "wait 2" "shot a.png" "key tab" "shot b.png" close`). With other copies of the game running, use `--pid <id>` and, for `dev_client.py`, your own `--pipe <name>`.

## The loop and the crew

Everyone's work runs through one field, `CrewMember::jobId`, and the systems never call each other about it:

| `jobId` | Means | Owner |
|---|---|---|
| `0` | free | whoever takes him next |
| `kWarehouseJobId` (1001, `warehouse_sim.cpp`) | carrying crates | `WarehouseSim` |
| `kLawJobId` (1002, `law_sim.h`) | the boss, running for the bolt-hole or caught | `LawSim` |
| `CitySim::kMissionJobId` (3095) | walking out, on a mission, wounded or arrested (by a raid too) | `CitySim` |
| `>= kLairFirstJobId` (1,000,000) | a dig or build job, with that job's id | `LairSim` |

- **Taking a man:** only one who's `away == false && jobId == 0`, setting your own value below `kLairFirstJobId`, and clearing it when done. The player may overwrite any job by sending him on a mission, so each owner checks every tick that the `jobId` is still theirs and puts the work back: a crate goes back where it came from (`ReleaseCarrier`), a lair job stays in the queue with its progress, and `LairSim::Tick` drops a taken man's claimed spot.
- **Who comes first:** the truck, then lair builds, then digs. `DispatchJobs` holds back `CarriersWanted(state)` free men while a truck is working. Nobody is pulled off a job for it: a man mid-dig finishes that tile. The truck only reports "nobody free" when no one's about to be.
- **Who never works:** the boss (each system checks `CrewRole`), and anyone `away`: on a mission's car, wounded or arrested until their `CrewOut` runs out.
- **Idling** is only `UpdateIdleCrew` in `lair_sim.cpp`, for men with `jobId == 0`; `wander 0` stops only that.
- **Going between levels:** every walk is `SendCharacterTo`, whose path crosses the stairs at any distance and re-plans when the world's revision changes. The boss stays below.
- **Being seen:** a view draws whoever is near its level by height, not `motion.level` (`LevelView::IsShownHere`), so someone on the stairs shows from both. A man leaving for a mission walks to the front door in the warehouse view, and his car appears on the map when the last one is out; the crew panel says "Heading out" until then.
- **Balance:** `city auto` measures it (see the table in the dev driver section), and `python py/law_tune.py` runs the auto players fast through raids and prints when each came. The auto players only order when the truck is free, so the truck queue doesn't move these numbers. Shipments are at the top of `warehouse_sim.cpp`, missions and heat at the top of `city_sim.cpp`, the lair's prices in `lair_sim.h` and `lair_tables.cpp`, the law's at the top of `law_sim.h`.

## The lair

Digging, rooms, doors, objects and the crew's work, on the underground level.

| Where | What |
|---|---|
| `src/game/sim/lair_tables.cpp` | The room and object tables. **A new room or object is one entry** (plus meshes in `py/create_meshes.py` and an icon in `py/create_ui_assets.py`); no code knows their names. Effects and uses are fields: `crewBeds`, `crateCapacity`, `protectedMoney`, `useSeconds`, `seat`, `bossSeat`. |
| `src/game/sim/lair_sim.h` | Tuning constants, state, and the API: `FindLairRoom*`, `GetLairRoomStatus`, `IsLairTileReachable`, `CrewCapacity`, `ExtraCrateCapacity` (shelves, which raise `CrateCapacity`), `ProtectedMoney`, and `Check*` / `Place*` / `Delete*`. |
| `src/game/sim/lair_build.cpp` | What may be placed or deleted, and doing it. |
| `src/game/sim/lair_sim.cpp` | `Tick`: the job queue, workers, keeping crew out of sites, idle crew using rooms. |
| `src/game/scenes/lair_view.*` | Drawing and the cutaway. |
| `src/game/scenes/lair_tools.*` | The build menu, room labels, the mouse on the world, ghosts. |

- **Everything placed is paid at once and becomes a `LairJob`.** Cancelling (right click) refunds it all; deleting something built refunds `kLairRefundFraction`. Builds go oldest first, then digs nearest first; each goes to the nearest free crew.
- **Solid things** are `Tile::solid` (`WorldGrid::SetTileSolid`); `IsWalkable` respects it, so paths do too.
- **Judging a change:** `lair_build.cpp` tests against `ProjectWorld` (the world with the whole queue finished) and floods from the foot of the stairs before and after. Anything reachable before and not after refuses it, with the reason in words.
- **Rooms:** a room's walls wait at 100% until nobody stands inside, so nobody is walled in; idle crew are sent out first. A wall shared with another room stays when one is deleted, as does any door in it.
- **The outermost ring of tiles can't be dug**, so walls never lie on the grid's border. The foot of the stairs stays outside rooms.
- **Cutaway** (`LairView::AllowedHeight`): along each sight line, an earth block or wall may only be as tall as keeps the first dug tile behind it in view. It's recomputed when the camera or world changes, and heights ease toward it. Picking uses the shown heights, so use `LairView::PickTile` or the dev driver's `tile`, not the ground plane, to aim at earth. A click aimed at a low block behind a tall one correctly hits the tall one.
- **Translucent drawing:** `MeshRenderer(device, MeshRenderStyle::Translucent)` blends by the color's alpha and doesn't write depth. The lair scene owns one for marks, blueprints and ghosts, drawn after the crew.
- **Room labels** sit on the room's nearest wall and nearly vanish while they'd cover someone (`BuildLabels`).
- The lair camera uses a narrow lens (`OrbitCamera::SetVerticalFov`, 32 degrees) from further away, so people at the screen's edges stand upright. `SetDefaultView` sets what `Reset` and the driver's `focus` go to. Middle-drag pans in every 3D view.

## The warehouse (ground level)

The front: fixed floor plan, a truck, a buy-and-sell loop. Tuning numbers are at the top of `src/game/sim/warehouse_sim.cpp`, and adding a kind of shipment is one line in `kShipmentKinds` (plus a color in `kCrateColors`, `warehouse_scene.cpp`).

- **Floor plan:** `src/game/world/warehouse_layout.*`, called from `MakeStartingWorld`: racks, stock, pallets, forklift, office, the big door, crate slots (`GetCrateSlot`). `TileKind::Fixture` is floor with something standing on it: drawn as floor, not walkable, and not picked by wandering. The player doesn't build here. The building's rectangle is in both `warehouse_layout.h` and `world_layout.cpp`: keep them equal.
- **The back room:** the stairs down are in a walled room in the far right corner (`kBackRoomX0`, `kBackRoomZ0`, `warehouse_layout.h`), not on the floor. Its door (`kBackRoomDoor`) faces a row of shelves (`kHidingShelves`, fixtures) with a one-tile passage between them, closed at the building's wall, so everyone walks in behind the shelves from the racks' side. `kStairTop` and `kStairFoot` (`world_layout.h`) are the one place the stairs are named; `kBasement`, `GetBasementCenter` and `kBoltHole` follow the foot, and the starting crew in `game_state.cpp` stand around it. Nothing treats the room specially but the view. With nobody in the room, the passage or the stairs (`IsAnyoneInBackRoom`: crew and law agents, by position), its walls stand whole, the doorway is plain wall (`WallLook::doorFilled`) and a roof (`DrawBackRoomRoof`, translucent style) covers it, since the camera looks down over walls of any height. When someone's inside, `m_backRoomOpen` eases to 1 (`EaseCutaway`, shared with the lair): the roof fades and `BackRoomWallLook` cuts down whichever walls face the camera, only as far as keeps the middle of the first tile's floor in view. It closes `kBackRoomHoldSeconds` after the last one leaves. The door is always the wall's color.
- **Sim:** `WarehouseSim` (`warehouse_sim.h`): `phase` (`TruckPhase`), `job` (Deliver / Collect), `hold` (NoCrew / NoRoom), `cargo`, `queue`, `stored`, `carriers`, `payments`. Orders: `OrderShipment(state, kind)`, `OrderShipOut(state)`, with `CanOrderShipment` / `CanShipOut` saying why not. Capacity: `CrateCapacity` = rack slots (24) + `ExtraCrateCapacity(state)`.
- **The queue:** one truck, and orders placed while it's out wait in `WarehouseSim::queue` (`QueuedTruck`, at most `kMaxQueuedTrucks`), first in, first out. The next sets off when the truck has driven off (`StartNextQueuedTruck`), and only then do its hours count, so `queue` is empty whenever `phase` is `Gone`. Money is paid when ordering; `CancelQueuedTruck` hands it back, and only queued orders can be cancelled. A queued Ship out costs nothing and takes what's there when it loads, so the panel promises no numbers until it sets off; with no crates by then it's dropped, and it's refused if none are expected (`GetCrateRoomAfterQueue` and `CanShipOut` work from the crates expected once everything ahead is done). A truck on `NoCrew` or `NoRoom` holds up the queue. The direct Ship out for a truck stuck on `NoRoom` skips the queue, even a full one. The queue is drawn in its own panel beside the main one, so the order buttons don't move.
- **Crew:** carriers are free non-boss crew (`away == false && jobId == 0`), up to 4, with `kWarehouseJobId` (see "The loop and the crew"). Clear or overwrite a carrier's `jobId` (or set `away`) to take them off: the crate goes back where it came from, and the job carries on with whoever else is free. If no one's free the truck waits, says so in the panel and news, and carries on by itself when someone is.
- **A full warehouse:** cargo that doesn't fit stays on the truck, which waits. It unloads the rest if capacity rises; or Ship out turns it round at once, ahead of the queue, taking what's aboard and crates from the racks to the buyer. So it can never deadlock.
- **The big door** is two wall edges (`kDockDoorTiles`) that `WarehouseSim` removes while it's open, so a closed door really blocks walking; paths re-plan through the world's revision. The shutter is drawn by the scene over the wall segments.
- **From outside:** read `state.crates` (count), `state.warehouse.stored` (`StoredCrate`: `position`, `tile`, `slot`, `kind`), `warehouse.cargo` and `warehouse.phase`. To take crates, lower `state.crates`: the newest go first and `Tick` keeps `stored` in step, including a carrier who was on the way to one. `AbandonWarehouse` is the raid's way of clearing all of it, the queue too. Outside is Pavement, with the front door at (19, 10), and the big door is open only while a truck is docked.
- **View:** `WarehouseScene` is fixed dressing built once (`BuildProps`) plus the truck, the door and the crates drawn from the sim. Near walls stay low and the others get `wall_upper`, chosen by the camera's direction every frame (`DrawTallWalls`, `kCutawayFacing`). The panel is `scenes/warehouse_panel.cpp`. Meshes: `py/create_warehouse_meshes.py` (writes `assets/meshes/warehouse/`, run like `create_meshes.py`). Icons: `py/create_warehouse_ui.py`.
- **Overflow:** crates beyond the racks, from `ExtraCrateCapacity`, go in 2 x 2 stacks on the overflow pallets in the warehouse (`kOverflowTiles`), however many there are, so raids can find them.

## The city map

`sim/city_sim.*`, `scenes/city_scene.*`.

- **Art and data come from one script:** `python py/create_city_art.py` draws `assets/ui/city/` (map, marker icons, cars, portrait layers) and generates `src/game/sim/city_map_data.h` (road graph, sites, districts, in map pixels). Never edit the header. The map is a table of blocks (`CELLS`), and a new place for missions is a line in `SITES`. Rerun `cook.bat` after it.
- **A mission is a row of `kMissions`** at the top of `city_sim.cpp`, with the other tuning numbers. A row names its `CityLocation`, and markers appear on free sites of that type. `MissionEffect::Hire` is the Recruit marker: nobody goes, the candidate joins.
- **Going:** `StartMission` charges the investment and overwrites the man's `jobId` with `CitySim::kMissionJobId`. He walks to the warehouse's door (found in the world: a Door edge from Floor to Pavement) and turns `away` there. When all are out the car drives (`MissionPhase`), the job runs, and `Resolve` pays or punishes. Back home, `ComeBack` clears `jobId` and walks him in.
- **Status:** `city.GetCrewStatus(member)`; wounded and arrested men stay `away` until their `CrewOut` runs out.
- **Heat:** only through `GameState::AddHeat`. Missions add it and `CitySim::TickHeat` lowers it once a game hour. `GetHeatChancePenalty` is the only effect it has here; what the law does about heat is in "The law". The HUD warns on any rise of more than 0.5.
- **Hiring** needs a free place: `state.crew.size() < CrewCapacity(state)`. The starting six are `kLairBaseCrewCapacity`, so a new game loses nothing, and the first hire needs a working Bunks room. The popup says so, and Recruit markers don't appear while it's full.

## The law

`sim/law_sim.*`, with every number at the top of `law_sim.h`. `hud/hud.cpp` shows evidence, escapes and a banner, and `hud/law_screen.*` is the screens.

- **Investigators and officers** are `LawAgent`s in `state.law.agents`: a `CharacterMotion` and a list of stops, not `state.crew`, so nothing gives them work. `LevelView` animates, draws and tags them; `LairView`'s doors swing for them; a room's walls wait for them to leave. `law.departedIds` tells `Game` to free their animation.
- **Visits:** by heat band, `kVisitEveryHours` and `kVisitorsAtOnce`. An investigator comes from the road, in by the front door, past the racks and the pallets, through the passage behind the shelves and the back room, down to the stairs' foot and up to `kLairStopsPerVisit` rooms he can reach, then back out. A stop he can't reach is skipped, and `kMaxVisitSeconds` ends any visit.
- **Evidence** (`LookAround`, `FileReport`): he notes crates within `kSightRadius`, evidence-worthy objects in the room he's standing in (`LairObjectType::evidence`), money outside the vault once he's below, and free soldiers not in a working room. Only his report counts, when he leaves. `ExtraCrateCapacity` crates count as on the shelves, worst first, at `kShelvedCrateFactor`. A clean report lowers evidence, as do `MissionDef::evidence` (Bribe) and `kEvidenceDecayPerHour` below `kEvidenceDecayHeat`.
- **The raid** (`TickRaid`): `heat >= kRaidHeat && evidence >= kRaidEvidence` starts `RaidPhase::Warning` and drops the game to 1x; it can't be called off. Then `RaidPhase::Police`: `kRaidOfficers` come in by the door, three stay up and the rest go below. When the first is indoors the boss runs for `kBoltHole` with `kLawJobId` and turns `away` there. It ends when all are in place, or at `kRaidMaxSeconds`.
- **Escape** (`MoveHouse`): money, `AbandonWarehouse` (crates, cargo, the truck and queued orders, unrefunded, with their own line on the report; payments due still come), half the built objects (`RemoveLairObjectForRaid`, no refund; unbuilt ones stay queued), men in the building arrested at `kRaidArrestChance` through `city.out`, so they come back by the usual `ComeBack`; the rest are placed inside the front door with `jobId` 0. Men on missions aren't touched. Heat and evidence go to 0.
- **Bust:** `IsFinalRaid()` once `raidsSurvived == kLawEscapes`. The boss stops where he is, an officer goes to him, and `BustBoss` puts up the last screen.
- **Screens:** while `law.screen` is up, `Simulation::Advance` returns at once, so no key, button or view switch can change anything. `Game` turns keys off and the screen's full-window panel takes the mouse. **New game:** `Simulation::NewGame` replaces `GameState` in place (scenes keep their reference), and `Game` rebuilds the scenes and HUD, because they hold caches.
- **New things that matter to the law** are fields: a new crime is `evidence` on an object or a kind in `kEvidencePerCrate`; `kStoredHeatPerHour` is what stored goods do to heat.

## Build configurations

`build.bat` sets everything per config; features are plain preprocessor defines:

| | dbg | opt | ship |
|---|---|---|---|
| Optimization / symbols | `/Od /Zi` | `/O2 /Zi` | `/O2 /Zi`, PDB kept next to the exe for crash dumps (not shipped; `/PDBALTPATH`) |
| Config define | `GAME_DBG` | `GAME_OPT` | `GAME_SHIP` |
| `GAME_LOGGING_ENABLED`, `GAME_ASSERTS_ENABLED`, `GAME_ASSERT_POPUP`, `GAME_IMGUI_ENABLED`, `GAME_IMGUI_CONSOLE_ENABLED`, `GAME_DEV_TOOLS_ENABLED` | yes | yes | no |
| Window default / VSync default | 1920x1080 windowed / off | same | borderless fullscreen on primary monitor / on |

- Our code compiles with `/W4 /WX /permissive- /EHsc /GR-` and `/D WIN32_LEAN_AND_MEAN NOMINMAX UNICODE`; vendored ImGui compiles separately with `/W3` and no `/WX`.
- Sources are gathered with a recursive glob of `src\*.cpp`; all `.obj` files go into one folder, so **`.cpp` basenames must be unique** across `src/`. Vendor code compiles into its own folders: `obj\imgui\` and `obj\ozz\` (`vendor/ozz/src`, `.cc`; some basenames clash with ours, e.g. `log`).
- ozz headers are passed to our code with `/external:I` + `/external:W0` so they don't trip `/W4 /WX`. The game links the ozz runtime (base + animation/runtime); `build_tools.bat` also compiles the offline part.
- Entry point is a plain `int main()` with `/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup`.

## Architecture

### Public vs internal headers
- `include/` holds public headers and is the only project include path (`/I include`, plus `/I vendor/imgui` when ImGui is enabled).
- Internal headers live next to their `.cpp` in `src/` and are included by **relative path** (e.g. `#include "win32_window.h"`, `#include "../sim/game_state.h"`). Keep it that way so they can't leak into other modules.

### Layering: interfaces + factories, context passing (no singletons)
- `include/platform/window.h` — `IWindow` (events, size, runtime mode switch, `RequestClose`) + `CreatePlatformWindow()`. Implemented by `src/platform/win32/Win32Window`.
- `include/render/render_device.h` — `IRenderDevice`, `IBuffer`, `IPipeline` + `CreateRenderDevice()`. Implemented in `src/render/d3d11/`. Resources are downcast with `static_cast` inside the D3D11 device.
- Only the factory functions name concrete classes. Higher levels receive interfaces by reference.
- `src/main.cpp` stays a few lines: `ParseLaunchOptions` → create window → create device → `Game(window, device, options).Run()`. Destruction order matters (game, then device, then window). `WM_CLOSE` only flags a close request; the HWND is destroyed in `~Win32Window`, after the device is gone.
- `Game` (`include/game/game.h`) owns frame logic: each frame it calls `device.Resize(window size)` (no-op if unchanged), runs the dev driver, starts the ImGui frame, advances the `Simulation`, updates the HUD then the active scene, `BeginFrame` → scene draw → game UI → ImGui render → `EndFrame` (present). While the window is dragged/resized (OS modal loop, `PumpEvents` doesn't return) the window drives `Frame()` from a timer via `IWindow::SetModalLoopCallback`. Unfocused, frames are throttled to ~30 fps.
- `IWindow` also exposes polled input: levels (`IsKeyDown(Key)`, `IsMouseButtonDown`), per-pump edges (`WasKeyPressed/Released`, `WasMouseButtonPressed/Released`: a press+release within one pump isn't lost), per-pump `GetMouseWheelDelta()`, mouse position, `HasFocus()`; add keys to the `Key` enum and `KeyFromVirtualKey` in `win32_window.cpp` (and `ParseKey` in the dev driver). Game code skips input ImGui is using (`ImGuiLayer::WantsMouse/WantsKeyboard`) and input game UI is using (`Ui::WantsMouse`).
- Per-monitor DPI v2: `WM_DPICHANGED` keeps the windowed client size in pixels, `ImGuiLayer` rescales its style and `Ui` its layout when `GetDpiScale()` changes, and borderless fullscreen covers the monitor the window is on.
- Platform-specific helpers used by platform-agnostic code are declared in internal `src/platform/platform_*.h` (log output, callstack/assert dialog/crash, cursor release, crash handler, system font lookup, the dev driver's pipe and PNG writer) and implemented in `src/platform/win32/`.
- Crashes: `main` installs `PlatformInstallCrashHandler()` (all configs): an unhandled exception writes a minidump to `%LOCALAPPDATA%\LairBuildingMafiaCPP\crashes\` and shows its path; open it with the matching `game.pdb`. Assert dialogs, `Fatal` and the crash handler release a mouse-look cursor capture first (`PlatformReleaseCursor`).

### Game layer (`src/game/`)

Each system lives in its own files, so parallel work doesn't collide.

- **Simulation** (`sim/`, no rendering or input): `GameState` (`game_state.h`) holds money, heat, the `GameClock` (pause, speeds 1-3, a game day is 2 real minutes at 1x), the `WorldGrid`, the crew list, the news list (`PostEvent`, `events`) and a seeded `random`. It's owned by `Simulation`, which `Game` owns, outside the views, and it runs whichever view is shown. Each system has its own pair of files: `warehouse_sim.*`, `lair_sim.*`, `city_sim.*`, each with a section above, and `law_sim.*` (investigators and raids). Add data to the struct and rules to its `Tick(GameState&, const SimStep&)`. `SimStep` is 0 while paused: `simSeconds` (real time x speed) for movement, `gameMinutes` for the day clock.
- **World** (`world/`): `WorldGrid` (`world_grid.h`) is two levels of tiles (`TileKind`: Earth, Floor, Pavement, Stairwell, Fixture; a free `room` index), walls and doors on tile edges (`GetEdge/SetEdge`), stairs (`AddStair`), `TileCenter`/`WorldToTile`, and a revision number that changes with it. World units are metres, a tile is 2 x 2, walls 2.4 high, the underground floor at y = -4. `world_path.h`: `FindTilePath` (A* over tiles and stair links, across levels), `FindWorldPath` (waypoints with corners cut), `HasClearLine`, `FindNearestWalkable`. `world_layout.cpp` is the starting layout. `WorldView` (`world_view.h`) draws a level. Dig and room rules are not here: they change tiles and edges through this API.
- **Characters** (`character/`): everyone is the same model, `assets/animated/soldier.blend`. `CharacterMotion` (`character_motion.h`, simulation data): position, level, path; `SendCharacterTo(motion, world, tile)`, `UpdateCharacterMotion` (re-plans when the world's revision changes). `CharacterView` (`character_view.h`, one shared, owned by `Game`): `Animate(id, motion, simSeconds)` cross-fades idle and run through one `Animator` layer per clip, with the run rate following the speed; `Draw(scope, id, motion, tint)`. **Per-character color**: the body material is near white, so `tint` is the body's color (the joints stay near black); the model isn't touched. Ids come from `GameState::nextCharacterId`. `CrewMember` has `role` (`CrewRole::Boss` / `Soldier`) and `tint`; the boss is the one saturated color (red), so keep other crew dull.
- **Views** (`scene.h`, `scenes/`): `IScene` (name, clear color, `Update(const SceneFrame&)`, `Draw(SceneRenderers)`, `OnDeactivate`, `RegisterCheats`), one per `ViewId`: `WarehouseScene`, `LairScene`, `CityScene`, built with `SceneServices`. Scenes keep no game state. `LevelView` (`scenes/level_view.h`) is what the 3D ones share: an `OrbitCamera` on a level, lighting, drawing the grid and the crew, `TileUnderCursor`. Add scene meshes in its `drawExtras`.
- **Camera** (`camera/orbit_camera.h`): `OrbitCamera` is a top-down orbit camera: WASD/arrows pan relative to the view, wheel zooms, Q/E turn, right-drag turns/tilts (the camera owns the right button: a press that moves past a small threshold orbits with the cursor hidden and locked, otherwise `WasRightClicked()` reports a click on release), `ScreenRay`/`ScreenToGround`/`WorldToScreen`.
- **Game UI** (`ui/`; ImGui is compiled out of ship, so game UI never uses it): `Ui` (`ui.h`) is immediate mode, called from `Update` in back-to-front order; what it builds is recorded and drawn over the scene. Logical pixels (96 DPI), scaled by the window's DPI: `Panel`, `Fill`, `Frame`, `Image`, `Label`, `LabelIn`, `MeasureText`, `WrapText`, `Button` (hover, press, selected, disabled), `IconButton`, `ProgressBar`, `Tooltip`, `IsHovered`, `Texture(path)` (cached), `GetContentTop()` (below the HUD). Panels take the mouse from what's under them. Build a scene's UI first, then check `ui.WantsMouse()` before using the mouse on the world. `UiPainter` (`ui_painter.h`) is the layer below: physical pixels, images and `Text`, flushed through a `SpriteBatch`. `UiFont` (`ui_font.h`): baked strikes of `assets/fonts/*.ttf`, the closest size at least as big is used, on whole pixels, so text is sharp at each DPI scale. Fonts are OFL licensed (`docs/PT_Sans_OFL.txt`).
- **HUD** (`hud/hud.*`): the thin bar on every view: money, heat, clock, pause and speeds (Space, 1/2/3), view switch (Tab or the buttons), newest event. In dev builds it sits below the ImGui menu bar.

### 2D rendering (`include/render/`, `src/render/`)
- Platform-agnostic, built only on `IRenderDevice`: `Camera2D` (y-up world, zoom = pixels per world unit, `ZoomAt` keeps the point under the cursor fixed), `SpriteBatch` and `LoadTexture` (`texture_file.h`).
- `SpriteBatch`: `SpriteBatch::Scope s = batch.Begin(camera); s.Draw(texture, sprite);` flushes when the scope ends (between `BeginFrame`/`EndFrame`). One instance (40 bytes) per sprite in a dynamic vertex buffer, expanded to a 4-vertex strip from `SV_VertexID` (`shaders/sprite.*.hlsl`); CPU-culled against the camera; one draw call per run of consecutive same-texture sprites; >64K sprites are uploaded in chunks.
- Premultiplied alpha everywhere: textures are premultiplied at cook time, sprite tints are straight alpha and premultiplied in the vertex shader, blend is `ONE / INV_SRC_ALPHA`.
- Pipelines carry their fixed-function state (`PipelineDesc`: vertex rate, topology, blend, cull, sampler filter); `Draw(const DrawDesc&)` binds b0 (both stages), t0/s0 (pixel shader). The D3D11 device only re-sets state that changed since the last draw (cache keyed by D3D object pointers, reset by `BeginFrame` and after ImGui renders).
- D3D11 device: max frame latency 1; `Present` reports a removed/reset device by name (`GetDeviceRemovedReason`) via `Fatal`, as does `CHECK_HR`. With the debug layer (dbg) and a debugger attached, errors break at the offending call; live objects are reported at shutdown.
- Shared render helpers: `Color` + `PackColorRGBA8` (`include/core/color.h`), `LoadShaderBytecode` (`include/render/shader_file.h`, fatal if missing), `LoadOrFatal(asset, path)` / `FatalMissingAsset` (`include/core/fatal.h`) for required assets.

### 3D rendering
- Math (`include/core/math.h`: `Vec2`, `Vec3`, `Mat4`, `kPi`, `DegToRad`/`RadToDeg`, `MulAffine`): left-handed, Y-up, +Z forward; column vectors (`p' = M * p`) stored row-major, translation in column 3. HLSL declares matrices `row_major` and uses `mul(M, v)`. D3D default winding: clockwise = front.
- `Camera3D` (yaw/pitch, perspective, depth [0, 1]) + `Frustum` (sphere test). `Mesh` / `LoadMesh` (`include/render/mesh.h`); `MeshCache` (`include/render/mesh_cache.h`) shares meshes by path between scenes.
- `MeshRenderer`: same scope pattern as `SpriteBatch` (`Begin(camera, lighting)` → `Draw(mesh, world, color, emissive)`). Instances are bucketed per mesh (opaque + depth tested, so order doesn't matter) → one `DrawIndexedInstanced` per mesh; frustum-culled by bounding sphere. Bucketing, upload and chunked draws are shared with `SkinnedMeshRenderer` through `InstanceBuckets<MeshT, InstanceT>` (`src/render/instance_buckets.h`). Instance = 3 world-matrix rows + color + emissive (stream 1); mesh vertices are stream 0. Static meshes have one color per instance, no vertex colors.
- Gouraud shading in `shaders/mesh.vs.hlsl`: one directional light + one point light (smooth range falloff) + ambient + Blinn-Phong specular, evaluated per vertex. Normals use the world matrix's cofactor (handles non-uniform scale/mirroring). No shadows.
- The device has a D32 depth buffer cleared by `BeginFrame`; pipelines opt in with `PipelineDesc::depth` (sprites leave it off).
- Lighting is shared: `DirectionalLight`/`PointLight`/`MeshLighting` (directional off by default) in `include/render/lighting.h`; `shaders/lighting.hlsli` (`TransformNormal`/`ShadeVertex`) and `src/render/lighting_constants.h` (`MakeLightingConstants`).
- Constant buffers are declared once for HLSL and C++: `shaders/<name>_cb.h` written with the `CBUFFER_BEGIN`/`CB_FLOAT*` macros of `shaders/cbuffer.h`, included by the shader and (by relative path, e.g. `"../../shaders/lighting_cb.h"`) by the C++ that fills it. Keep members aligned to 16-byte registers and static_assert the size in C++.

### Skeletal animation
- `include/anim/skeletal_animation.h`: `Skeleton`, `AnimationClip` (+ `LoadSkeleton`/`LoadAnimationClip`) and `Animator` (per character: samples up to 4 weighted `AnimationLayer`s, blends, outputs model-space `Mat4`s). ozz types stay in `src/anim/` behind pimpls; public headers never include ozz.
- `SkinnedMesh` (`.skin`, joint indices/inverse binds in ozz joint order, max 256 joints, per-vertex RGBA8 material color multiplied by the instance color) + `SkinnedMeshRenderer`: same scope pattern; `Draw(mesh, world, animator.GetModelMatrices(), color)` appends `world * model * inverseBind` (affine products, `MulAffine`) per joint to a per-scope palette (structured buffer at VS t0); instances (palette offset + color) are bucketed per mesh, one draw per mesh. 4-influence linear blend skinning in `shaders/skinned.vs.hlsl`. Culling uses the bind-pose sphere x1.5.
- Each `Animator` layer index keeps its own ozz sampling cache (allocated on first use), so weights crossing zero don't invalidate other layers' caches. `Evaluate` is safe to run in parallel across animators.

### Dev-only features are compiled out, not disabled at runtime
- `include/core/log.h`: `LOG_INFO/LOG_WARN/LOG_ERR` (printf-style). Output goes to OutputDebugString, plus the redirected stdout or the parent console. `LogAddListener` registers a sink and **replays the last 1024 lines**; the ImGui console relies on this to show startup logs.
- `include/core/assert.h`: `GAME_ASSERT(cond, ["fmt", args...])` and `GAME_ASSERT_CRITICAL`. The optional message must start with a string literal (the macro uses `"" __VA_ARGS__`). With the popup, a TaskDialog offers Retry (re-evaluates the condition) / Continue / Abort (exit code 3); critical asserts have no Continue, and without the popup they crash via `__fastfail`.
- When disabled, both macro families expand to `sizeof(...)` expressions: arguments are type-checked and count as used (so `/WX` doesn't fail on unused variables in ship) but generate no code.
- ImGui hooks on `IWindow`/`IRenderDevice` are declared under `#if defined(GAME_IMGUI_ENABLED)`, so public headers never include ImGui. The dev driver's hooks likewise under `GAME_DEV_TOOLS_ENABLED` (`DevInject*`, `CaptureBackBuffer`).

### Developer UI (`include/debug/`, `src/debug/`)
- `ImGuiLayer` owns the ImGui context and drives the window/device backends. It loads the system font Consolas (15 px) via `PlatformFindSystemFont` and disables `imgui.ini`.
- `DevUI` owns the main menu bar (File/Exit, Tools/DebugPad, Frame Counter, Console), the `DebugPad`, the frame-counter overlay and the `DebugConsole`. While the console is open (toggled by the key under Esc, `ImGuiKey_GraveAccent`), DevUI draws nothing else.
- Cheats: register a category with `DebugPad::RegisterCategory(name, std::function<void()> draw)`, reaching the pad via `DevUI::GetDebugPad()` or `IScene::RegisterCheats`. "View" switches views, "Simulation" (`src/game/dev/sim_cheats.cpp`) sets money, heat, pause, speed, wandering and lists the crew; "Window" (`src/debug/window_cheats.cpp`) is the fullscreen toggle and resolution; each 3D view has a camera category.
- Esc does not quit the game; use File → Exit (or Alt+F4, the only way in ship).

### Assets / cooking
- `cook.bat` checks for fxc/python and runs `py/cook.py`, which cooks only stale outputs (older than their source or than the script/tool/includes producing them); `--force` re-cooks everything.
- Shaders live in `shaders/` and follow the naming rule `<name>.<stage>.hlsl` (stage `vs` → `vs_5_0`, `ps` → `ps_5_0`), entry point `main`. Shared `*.hlsli` / `*.h` files are includes only (changing one recompiles every shader). Adding a shader requires no script changes.
- Shaders cook to `__generated\data\shaders\<name>.<stage>.cso`: optimized (`/O3`) by default, `cook.bat --debug-shaders` for `/Od /Zi` (for PIX/RenderDoc; the mode is remembered in `shader_mode.txt`, switching recompiles all).
- Textures: `assets/<path>.png` → `__generated\data\<path>.tex`, packed by `py/pack_texture.py` (needs Python 3 + Pillow on PATH; called once per file). `.tex` = 24-byte header (`TextureFileHeader`) + full mip chain of RGBA8 premultiplied pixels; keep the script and `include/render/texture_file.h` in sync.
- Fonts: `assets/fonts/<name>.ttf` → `__generated\data\fonts\<name>.font` + `.tex`, baked by `py/pack_font.py` (sizes in `STRIKE_SIZES`); keep its layout and `src/game/ui/ui_font.cpp` in sync. Use OFL licensed fonts and keep the licence in `docs/`.
- Meshes: `assets/<path>.blend` → `__generated\data\<path>.mesh`, exported by `py/pack_mesh.py` running inside Blender. All mesh objects are merged with transforms/modifiers applied; corner normals; converted to left-handed Y-up with reversed winding. Keep the script and `include/render/mesh.h` in sync.
- Skeletal: `assets/animated/<path>.blend` (one armature, meshes with an Armature modifier, actions) → `py/pack_skeletal.py` in Blender → `__generated\intermediate\<path>.skraw` → `__generated\tools\anim_cook.exe` (ozz offline: SkeletonBuilder, AnimationOptimizer, AnimationBuilder; each face's material base color becomes a vertex color, white without material) → `__generated\data\animated\<path>.skel`, `.<action>.anim`, `.skin`. The .skraw layout is documented in `pack_skeletal.py` and read by `tools/anim_cook/anim_cook.cpp`; `.skin` is defined by `include/render/skinned_mesh.h` (shared by tool and game).
- Placeholder assets are made by script and committed. Meshes: `py/create_meshes.py` in Blender (`blender --background --factory-startup --python py/create_meshes.py [-- name ...]`) writes `assets/meshes/**/*.blend`; add a function of boxes and a `MESHES` entry. 2D: `python py/create_ui_assets.py` (Pillow) writes `assets/ui/`; add a function and an `ICONS` entry. The soldier: `py/create_soldier.py` in Blender builds `assets/animated/soldier.blend` (Mixamo rig; idle, run, die clips; near-white body and near-black joints) from the FBXs in `raw/`.
- At runtime, asset paths are **relative to the working directory**, which is `__generated\data` (a shipped build puts exe and data in one folder). Load files with `ReadFileBytes` (`include/core/file.h`) and pass bytecode to `PipelineDesc{ ShaderBytecode vertexShader, pixelShader, ... }`. There is no runtime HLSL compilation in engine code (only ImGui's DX11 backend uses `d3dcompiler`).

## Conventions
- `__generated/` is build/cook output only; never put sources there.
- `vendor/imgui` is an unmodified copy of Dear ImGui (version in `vendor/imgui/VERSION.txt`); don't edit it.
- `vendor/ozz` is an unmodified subset of ozz-animation (version and contents in `vendor/ozz/VERSION.txt`); don't edit it.
- Source files use **CRLF** line endings; keep them consistent when editing with scripts. `.gitattributes` turns off git's conversion.
- Fatal, unrecoverable errors go through `Fatal(msg, code)` (`include/core/fatal.h`); D3D calls use the local `CHECK_HR` macro.
- Keep it simple: game content is data in tables; no general frameworks beyond what's above.
