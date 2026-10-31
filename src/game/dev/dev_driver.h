#pragma once

#if defined(GAME_DEV_TOOLS_ENABLED)

#include "../world/world_grid.h"
#include "game/launch_options.h"
#include "../../platform/platform_dev.h"
#include "platform/window.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class IRenderDevice;
class OrbitCamera;
class Simulation;

// Lets a script or an agent drive the game and look at it (dbg and opt; compiled out of ship).
// Commands come from -run, -script or a named pipe (-pipe, see py/dev_client.py); they run one at a
// time on the game's own thread, each finishing before the next starts. On the pipe each gets a
// reply: "ok ..." or "err ...". The commands are listed in CLAUDE.md and at Execute() in the .cpp.
struct DevDriverHooks
{
    std::function<const char*()>                  getView;
    std::function<bool(const std::string&)>       setView;
    std::function<OrbitCamera*()>                 getOrbitCamera;
    std::function<float(const TileCoord&)>        getTileSurfaceHeight;
    std::function<void(bool)>                     setDevUiVisible; // empty without ImGui
    std::function<void(bool)>                     setDebugPadOpen; // empty without ImGui
};

class DevDriver
{
public:
    DevDriver(const DevOptions& options, IWindow& window, IRenderDevice& device, Simulation& simulation, DevDriverHooks hooks);
    ~DevDriver();

    DevDriver(const DevDriver&)            = delete;
    DevDriver& operator=(const DevDriver&) = delete;

    // Takes new commands, finishes waits and runs what's next. Call at the start of each frame,
    // after the window pumped events.
    void BeginFrame();
    // Takes a pending screenshot. Call when the frame is drawn, before presenting.
    void EndFrame();

    // Simulated input is in use: real input and ImGui's opinion of it don't count.
    bool IsInjecting() const { return m_injecting; }

private:
    struct Queued
    {
        std::string text;
        bool        replyOnPipe = false;
    };

    void PollPipe();
    // Runs a command. Returns false if it isn't finished yet (a wait, a key tap, a screenshot).
    bool Execute(const std::string& text, std::string& reply);
    void Finish(const std::string& reply);

    IWindow&       m_window;
    IRenderDevice& m_device;
    Simulation&    m_simulation;
    DevDriverHooks m_hooks;

    std::unique_ptr<IDevPipe> m_pipe;
    std::deque<Queued>        m_queue;
    bool                      m_commandRunning = false;
    bool                      m_replyOnPipe    = false;
    bool                      m_injecting      = false;

    // What the running command waits for.
    double      m_waitSeconds = 0.0;
    int         m_waitFrames  = 0;
    std::string m_screenshotPath;
    std::string m_pendingReply;
    // Second half of a tap: released on the next frame.
    bool        m_releaseKey   = false;
    Key         m_key          = Key::Count;
    bool        m_releaseMouse = false;
    MouseButton m_button       = MouseButton::Left;

    uint64_t m_lastTicks = 0;
};

#endif
