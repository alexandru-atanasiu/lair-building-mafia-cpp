# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

"Capo di Tutti Capi" (working title): an Evil Genius style lair builder about a mob family, in real 3D. Windows-only C++20 / Direct3D 11, with skeletal animation (ozz-animation, GPU skinning), instanced 3D meshes with Gouraud lighting, instanced 2D sprites, game UI with baked bitmap fonts, and developer tooling built on Dear ImGui. No CMake/MSBuild: everything is driven by batch scripts that call `cl`, `link` and `fxc` directly. The engine parts were ported from an earlier in-house project. Tentative design: `docs/DESIGN.md` (its "As built" section describes an earlier C# version and is only background).

Three views share one simulation and one clock: the **warehouse** (ground level, 3D), the **lair** (underground level, 3D) and the **city** (2D map). So far they are stubs: gameplay is added on the seams described under "Game layer".

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
| `key <name>`, `keydown <name>`, `keyup <name>` | tap / press / release (`tab space escape shift control 1 2 3 w a s d q e left right up down`) |
| `move <x> <y>`, `click <x> <y> [left\|right\|middle]`, `mousedown [b]`, `mouseup [b]`, `wheel <notches>` | mouse |
| `tile <ground\|under> <x> <z>` | replies `ok <x> <y>`: where that tile's center is on screen in the current view; use it to click on the world |
| `focus <ground\|under> <x> <z>` | point the current view's camera at a tile |
| `goto <crew id> <ground\|under> <x> <z>` | send a crew member somewhere (`wander 0` first, or they soon wander off) |
| `wander 0\|1` | idle crew wander around, or stay put |
| `money <n>`, `heat <n>`, `speed <1\|2\|3>`, `pause <0\|1>` | simulation |
| `wait <seconds>`, `frames <n>` | wait (real time, frames) before the next command |
| `screenshot <path>` | PNG of the next finished frame. Use an absolute path (`dev_client.py` makes them absolute); the game's working directory is `__generated\data` |
| `devui 0\|1`, `debugpad 0\|1` | hide / show ImGui, close / open the DebugPad |
| `order cigarettes\|booze\|contraband`, `shipout` | the warehouse panel's buttons |
| `crates <n>`, `crewjob <crew id> <n>` | set the stored crates; set a crew member's `jobId` (non-zero makes them busy) |
| `state` | replies `ok view=Lair money=5000 heat=0 day=1 hour=8 ... crates=0 capacity=24 truck=gone hold=none cargo=0 carriers=Sal*,Tony paymentsdue=0 ... crew1=Don Carlo:boss:under:19:24:idle ...` (`*` = holding a crate) |
| `quit` | close the game |

Add a command in `DevDriver::Execute`. Ship has no driver: `py/capture_window.py` screenshots a running window from outside and posts clicks and keys to it (`run.bat ship -windowed`, then `python py\capture_window.py "wait 2" "shot a.png" "key tab" "shot b.png" close`). With other copies of the game running, use `--pid <id>` and, for `dev_client.py`, your own `--pipe <name>`.

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

- **Simulation** (`sim/`, no rendering or input): `GameState` (`game_state.h`) holds money, heat, the `GameClock` (pause, speeds 1-3, a game day is 2 real minutes at 1x), the `WorldGrid`, the crew list, the news list (`PostEvent`, `events`) and a seeded `random`. It's owned by `Simulation`, which `Game` owns, outside the views, and it runs whichever view is shown. Each later system has its own pair of files, already hooked up: `warehouse_sim.*` (truck), `lair_sim.*` (dig, rooms, objects, jobs; for now idle crew wander), `city_sim.*` (missions), `law_sim.*` (investigators, raids). Add data to the struct and rules to its `Tick(GameState&, const SimStep&)`. `SimStep` is 0 while paused: `simSeconds` (real time x speed) for movement, `gameMinutes` for the day clock.
- **World** (`world/`): `WorldGrid` (`world_grid.h`) is two levels of tiles (`TileKind`: Earth, Floor, Pavement, Stairwell; a free `room` index), walls and doors on tile edges (`GetEdge/SetEdge`), stairs (`AddStair`), `TileCenter`/`WorldToTile`, and a revision number that changes with it. World units are metres, a tile is 2 x 2, walls 2.4 high, the underground floor at y = -4. `world_path.h`: `FindTilePath` (A* over tiles and stair links, across levels), `FindWorldPath` (waypoints with corners cut), `HasClearLine`, `FindNearestWalkable`. `world_layout.cpp` is the starting layout. `WorldView` (`world_view.h`) draws a level. Dig and room rules are not here: they change tiles and edges through this API.
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

## The warehouse (ground level)

The front: fixed floor plan, a truck, a buy-and-sell loop. Tuning numbers are at the top of `src/game/sim/warehouse_sim.cpp`, and adding a kind of shipment is one line in `kShipmentKinds` (plus a color in `kCrateColors`, `warehouse_scene.cpp`).

- **Floor plan:** `src/game/world/warehouse_layout.*`, called from `MakeStartingWorld`: racks, stock, pallets, forklift, office, the big door, crate slots (`GetCrateSlot`). `TileKind::Fixture` is floor with something standing on it: drawn as floor, not walkable, and not picked by wandering. The player doesn't build here. The building's rectangle is in both `warehouse_layout.h` and `world_layout.cpp`: keep them equal.
- **Sim:** `WarehouseSim` (`warehouse_sim.h`): `phase` (`TruckPhase`), `job` (Deliver / Collect), `hold` (NoCrew / NoRoom), `cargo`, `stored`, `carriers`, `payments`. Orders: `OrderShipment(state, kind)`, `OrderShipOut(state)`, with `CanOrderShipment` / `CanShipOut` saying why not. Capacity: `CrateCapacity` = rack slots (24) + `ExtraCrateCapacity(state)`.
- **Crew:** carriers are free non-boss crew (`away == false && jobId == 0`), up to 4, with `jobId = 1000001`. Clear or overwrite a carrier's `jobId` (or set `away`) to take them off: the crate goes back where it came from, and the job carries on with whoever else is free. If no one's free the truck waits, says so in the panel and news, and carries on by itself when someone is.
- **A full warehouse:** cargo that doesn't fit stays on the truck, which waits. It unloads the rest if capacity rises; or Ship out turns it round, taking what's aboard and crates from the racks to the buyer. So it can never deadlock.
- **The big door** is two wall edges (`kDockDoorTiles`) that `WarehouseSim` removes while it's open, so a closed door really blocks walking; paths re-plan through the world's revision. The shutter is drawn by the scene over the wall segments.
- **For `law_sim`:** read `state.crates` (count), `state.warehouse.stored` (`StoredCrate`: `position`, `tile`, `slot`, `kind`), `warehouse.cargo` and `warehouse.phase`. To confiscate, lower `state.crates`: the newest go first and `Tick` keeps `stored` in step, including a carrier who was on the way to one. A law character can walk the ground level normally: outside is Pavement, with the front door at (19, 10), and the big door is open only while a truck is docked.
- **View:** `WarehouseScene` is fixed dressing built once (`BuildProps`) plus the truck, the door and the crates drawn from the sim. Near walls stay low and the others get `wall_upper`, chosen by the camera's direction every frame (`DrawTallWalls`, `kCutawayFacing`). The panel is `scenes/warehouse_panel.cpp`. Meshes: `py/create_warehouse_meshes.py` (writes `assets/meshes/warehouse/`, run like `create_meshes.py`). Icons: `py/create_warehouse_ui.py`.
- **`ExtraCrateCapacity`** is a stub returning 0 in `lair_sim.cpp`. `lair-building` defines the real one (`lair_build.cpp`): delete the stub at merge. Crates beyond the racks go in 2 x 2 stacks on the overflow pallets in the warehouse (`kOverflowTiles`), however many there are, so raids can find them.

## Conventions
- `__generated/` is build/cook output only; never put sources there.
- `vendor/imgui` is an unmodified copy of Dear ImGui (version in `vendor/imgui/VERSION.txt`); don't edit it.
- `vendor/ozz` is an unmodified subset of ozz-animation (version and contents in `vendor/ozz/VERSION.txt`); don't edit it.
- Source files use **CRLF** line endings; keep them consistent when editing with scripts. `.gitattributes` turns off git's conversion.
- Fatal, unrecoverable errors go through `Fatal(msg, code)` (`include/core/fatal.h`); D3D calls use the local `CHECK_HR` macro.
- Keep it simple: game content is data in tables; no general frameworks beyond what's above.
