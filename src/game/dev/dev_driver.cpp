#include "dev_driver.h"

#if defined(GAME_DEV_TOOLS_ENABLED)

#include "../camera/orbit_camera.h"
#include "../sim/simulation.h"
#include "core/log.h"
#include "render/render_device.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace
{
    bool ParseKey(const std::string& name, Key& key)
    {
        static const struct { const char* name; Key key; } kKeys[] = {
            { "left", Key::Left }, { "right", Key::Right }, { "up", Key::Up }, { "down", Key::Down },
            { "w", Key::W }, { "a", Key::A }, { "s", Key::S }, { "d", Key::D }, { "q", Key::Q }, { "e", Key::E },
            { "shift", Key::Shift }, { "control", Key::Control }, { "tab", Key::Tab }, { "space", Key::Space },
            { "escape", Key::Escape }, { "1", Key::Num1 }, { "2", Key::Num2 }, { "3", Key::Num3 },
        };
        for (const auto& entry : kKeys)
        {
            if (name == entry.name)
            {
                key = entry.key;
                return true;
            }
        }
        return false;
    }

    bool ParseButton(const std::string& name, MouseButton& button)
    {
        if (name.empty() || name == "left")
            button = MouseButton::Left;
        else if (name == "right")
            button = MouseButton::Right;
        else if (name == "middle")
            button = MouseButton::Middle;
        else
            return false;
        return true;
    }

    bool ParseLevel(const std::string& name, WorldLevel& level)
    {
        if (name == "ground")
            level = WorldLevel::Ground;
        else if (name == "under")
            level = WorldLevel::Underground;
        else
            return false;
        return true;
    }

    std::string Trim(const std::string& s)
    {
        const size_t begin = s.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos)
            return {};
        return s.substr(begin, s.find_last_not_of(" \t\r\n") - begin + 1);
    }

    std::string DescribeState(Simulation& simulation, const char* view, IWindow& window)
    {
        GameState& state = simulation.GetState();
        std::ostringstream out;
        out << "view=" << view << " money=" << state.money << " heat=" << state.heat << " day=" << state.clock.GetDay()
            << " hour=" << state.clock.GetHour() << " minute=" << state.clock.GetMinute()
            << " speed=" << state.clock.GetSpeed() << " paused=" << (state.clock.IsPaused() ? 1 : 0)
            << " width=" << window.GetWidth() << " height=" << window.GetHeight() << " dpi=" << window.GetDpiScale()
            << " events=" << state.events.size();
        if (!state.events.empty())
            out << " lastevent=\"" << state.events.back().text << "\"";
        for (const CrewMember& member : state.crew)
        {
            const TileCoord tile = state.world.WorldToTile(member.motion.level, member.motion.position);
            out << " crew" << member.id << "=" << member.name << ":" << (member.role == CrewRole::Boss ? "boss" : "soldier") << ":"
                << (member.motion.level == WorldLevel::Ground ? "ground" : "under") << ":" << tile.x << ":" << tile.z << ":"
                << (member.away ? "away" : (member.motion.IsMoving() ? "walking" : "idle"));
        }
        return out.str();
    }
}

DevDriver::DevDriver(const DevOptions& options, IWindow& window, IRenderDevice& device, Simulation& simulation, DevDriverHooks hooks)
    : m_window(window)
    , m_device(device)
    , m_simulation(simulation)
    , m_hooks(std::move(hooks))
{
    if (options.dpiScale > 0.0f)
        m_window.DevSetDpiScale(options.dpiScale);
    if (options.hideDevUi && m_hooks.setDevUiVisible)
        m_hooks.setDevUiVisible(false);
    if (!options.startView.empty() && !m_hooks.setView(options.startView))
        LOG_WARN("-view: no view named '%s'", options.startView.c_str());

    if (!options.scriptPath.empty())
    {
        std::ifstream file(options.scriptPath);
        if (!file)
            LOG_ERR("-script: can't read '%s'", options.scriptPath.c_str());
        std::string line;
        while (std::getline(file, line))
            m_queue.push_back({ line, false });
    }
    std::stringstream commands(options.commands);
    std::string       command;
    while (std::getline(commands, command, ';'))
        m_queue.push_back({ command, false });

    if (!options.pipeName.empty())
    {
        m_pipe = CreateDevPipe(options.pipeName.c_str());
        // Driven from outside: nobody at the keyboard should interfere from the first frame on.
        m_injecting = true;
    }
    if (!m_queue.empty())
        m_injecting = true;
    if (m_injecting)
        m_window.DevInjectMouseMove(m_window.GetMouseX(), m_window.GetMouseY());
}

DevDriver::~DevDriver() = default;

void DevDriver::PollPipe()
{
    if (!m_pipe)
        return;
    std::string line;
    while (m_pipe->ReadLine(line))
        m_queue.push_back({ line, true });
}

void DevDriver::Finish(const std::string& reply)
{
    LOG_INFO("[dev] -> %s", reply.c_str());
    if (m_replyOnPipe && m_pipe)
        m_pipe->Write(reply + "\n");
    m_commandRunning = false;
}

void DevDriver::BeginFrame()
{
    using Clock = std::chrono::steady_clock;
    const uint64_t ticks = (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(Clock::now().time_since_epoch()).count();
    const double   delta = m_lastTicks ? (double)(ticks - m_lastTicks) * 1e-6 : 0.0;
    m_lastTicks = ticks;

    PollPipe();

    // Second half of a tap, one frame later: the game saw the press with the button down.
    if (m_releaseKey)
    {
        m_window.DevInjectKey(m_key, false);
        m_releaseKey = false;
        m_waitFrames = 1;
        return;
    }
    if (m_releaseMouse)
    {
        m_window.DevInjectMouseButton(m_button, false);
        m_releaseMouse = false;
        m_waitFrames = 1;
        return;
    }

    if (m_commandRunning)
    {
        if (m_waitFrames > 0)
        {
            --m_waitFrames;
            return;
        }
        if (m_waitSeconds > 0.0)
        {
            m_waitSeconds -= delta;
            return;
        }
        if (!m_screenshotPath.empty())
            return; // EndFrame takes it
        Finish(m_pendingReply);
    }

    // Commands that take no time run together, up to the first that does.
    while (!m_commandRunning && !m_queue.empty())
    {
        Queued next = std::move(m_queue.front());
        m_queue.pop_front();
        const std::string text = Trim(next.text);
        if (text.empty() || text[0] == '#')
            continue;

        LOG_INFO("[dev] %s", text.c_str());
        m_replyOnPipe    = next.replyOnPipe;
        m_commandRunning = true;
        m_pendingReply   = "ok";
        if (Execute(text, m_pendingReply))
            Finish(m_pendingReply);
    }
}

void DevDriver::EndFrame()
{
    if (m_screenshotPath.empty())
        return;

    std::vector<uint8_t> rgba;
    uint32_t             width = 0, height = 0;
    if (!m_device.CaptureBackBuffer(rgba, width, height))
        m_pendingReply = "err can't read the back buffer";
    else if (!PlatformWritePng(m_screenshotPath.c_str(), rgba.data(), width, height))
        m_pendingReply = "err can't write " + m_screenshotPath;
    else
        m_pendingReply = "ok " + m_screenshotPath;
    m_screenshotPath.clear();
}

// Commands. Coordinates are client pixels (the state command gives the window's size):
//   view <warehouse|lair|city>   switch view
//   key <name>                   tap a key (see ParseKey)
//   keydown <name> / keyup <name>
//   move <x> <y>                 move the mouse
//   click <x> <y> [left|right|middle]  move and tap a button (the press and release take a frame each)
//   mousedown [button] / mouseup [button]
//   wheel <notches>
//   wait <seconds> / frames <count>
//   screenshot <path>            PNG of the next finished frame; use an absolute path
//   money <n> / heat <n> / speed <1|2|3> / pause <0|1>
//   goto <crew id> <ground|under> <x> <z>   send a crew member to a tile
//   tile <ground|under> <x> <z>  replies "ok <x> <y>": where a tile is on screen in this view
//   focus <ground|under> <x> <z> point this view's camera at a tile
//   wander <0|1>                 idle crew wander, or stay put
//   devui <0|1>                  show the dev UI
//   debugpad <0|1>               open the DebugPad
//   state                        replies "ok view=... money=... crew1=Don Carlo:boss:under:19:19:idle ..."
//   quit
bool DevDriver::Execute(const std::string& text, std::string& reply)
{
    m_injecting = true;

    std::istringstream in(text);
    std::string        word;
    in >> word;

    auto fail = [&](const std::string& message) {
        reply = "err " + message;
        LOG_WARN("[dev] %s: %s", text.c_str(), message.c_str());
        return true;
    };

    GameState& state = m_simulation.GetState();

    if (word == "view")
    {
        std::string name;
        in >> name;
        if (!m_hooks.setView(name))
            return fail("no such view");
    }
    else if (word == "key" || word == "keydown" || word == "keyup")
    {
        std::string name;
        in >> name;
        Key key = Key::Count;
        if (!ParseKey(name, key))
            return fail("unknown key");
        m_window.DevInjectKey(key, word != "keyup");
        if (word == "key")
        {
            m_key        = key;
            m_releaseKey = true;
            return false;
        }
    }
    else if (word == "move" || word == "click")
    {
        int x = 0, y = 0;
        if (!(in >> x >> y))
            return fail("expected x y");
        std::string buttonName;
        in >> buttonName;
        m_window.DevInjectMouseMove(x, y);
        if (word == "click")
        {
            MouseButton button = MouseButton::Left;
            if (!ParseButton(buttonName, button))
                return fail("unknown button");
            m_window.DevInjectMouseButton(button, true);
            m_button       = button;
            m_releaseMouse = true;
            return false;
        }
    }
    else if (word == "mousedown" || word == "mouseup")
    {
        std::string name;
        in >> name;
        MouseButton button = MouseButton::Left;
        if (!ParseButton(name, button))
            return fail("unknown button");
        m_window.DevInjectMouseButton(button, word == "mousedown");
    }
    else if (word == "wheel")
    {
        float notches = 0.0f;
        in >> notches;
        m_window.DevInjectMouseWheel(notches);
    }
    else if (word == "wait")
    {
        in >> m_waitSeconds;
        return false;
    }
    else if (word == "frames")
    {
        in >> m_waitFrames;
        return false;
    }
    else if (word == "screenshot")
    {
        std::getline(in, m_screenshotPath);
        m_screenshotPath = Trim(m_screenshotPath);
        if (m_screenshotPath.empty())
            return fail("expected a path");
        return false;
    }
    else if (word == "money")
    {
        long long money = 0;
        in >> money;
        state.money = money;
    }
    else if (word == "heat")
    {
        float heat = 0.0f;
        in >> heat;
        state.heat = 0.0f;
        state.AddHeat(heat);
    }
    else if (word == "speed")
    {
        int speed = 1;
        in >> speed;
        if (speed < 1 || speed > GameClock::kMaxSpeed)
            return fail("speed is 1..3");
        state.clock.SetSpeed(speed);
    }
    else if (word == "pause")
    {
        int paused = 1;
        in >> paused;
        state.clock.SetPaused(paused != 0);
    }
    else if (word == "goto")
    {
        uint32_t    id = 0;
        std::string level;
        TileCoord   tile;
        in >> id >> level >> tile.x >> tile.z;
        CrewMember* member = state.FindCrew(id);
        if (!member)
            return fail("no such crew member");
        if (!ParseLevel(level, tile.level))
            return fail("level is ground or under");
        if (!SendCharacterTo(member->motion, state.world, tile))
            return fail("no path");
    }
    else if (word == "tile" || word == "focus")
    {
        std::string level;
        TileCoord   tile;
        in >> level >> tile.x >> tile.z;
        if (!ParseLevel(level, tile.level))
            return fail("level is ground or under");
        OrbitCamera* camera = m_hooks.getOrbitCamera();
        if (!camera)
            return fail("this view has no 3D camera");
        if (word == "focus")
        {
            camera->Reset(state.world.TileCenter(tile));
        }
        else
        {
            Vec2 screen;
            if (!camera->WorldToScreen(state.world.TileCenter(tile), screen))
                return fail("behind the camera");
            char position[64];
            snprintf(position, sizeof(position), "ok %d %d", (int)screen.x, (int)screen.y);
            reply = position;
        }
    }
    else if (word == "wander")
    {
        int enabled = 1;
        in >> enabled;
        state.lair.wanderEnabled = enabled != 0;
    }
    else if (word == "devui")
    {
        int visible = 1;
        in >> visible;
        if (m_hooks.setDevUiVisible)
            m_hooks.setDevUiVisible(visible != 0);
    }
    else if (word == "debugpad")
    {
        int open = 1;
        in >> open;
        if (m_hooks.setDebugPadOpen)
            m_hooks.setDebugPadOpen(open != 0);
    }
    else if (word == "state")
    {
        reply = "ok " + DescribeState(m_simulation, m_hooks.getView(), m_window);
    }
    else if (word == "quit")
    {
        m_window.RequestClose();
    }
    else
    {
        return fail("unknown command");
    }
    return true;
}

#endif
