#pragma once

#include "platform/window.h"
#include "render/render_device.h"

#if defined(GAME_DEV_TOOLS_ENABLED)
#include <string>

// The dev driver's arguments (dbg and opt only), see src/game/dev/dev_driver.h.
struct DevOptions
{
    std::string startView;  // -view warehouse|lair|city
    std::string pipeName;   // -pipe <name>: listen for commands on \\.\pipe\<name>
    std::string scriptPath; // -script <file>: run the commands in a file, one per line
    std::string commands;   // -run "<command>; <command>": run these commands
    bool        hideDevUi = false; // -nodevui: no ImGui at all, looks like ship
    float       dpiScale  = 0.0f;  // -dpiscale <scale>: pretend the monitor has this DPI scale
};
#endif

struct LaunchOptions
{
    WindowDesc       window;
    RenderDeviceDesc renderDevice;
#if defined(GAME_DEV_TOOLS_ENABLED)
    DevOptions       dev;
#endif
};

// Settings for this build config, with command-line overrides:
//   -windowed / -fullscreen
//   -vsync / -novsync
//   dbg/opt only: the DevOptions above, and -size <width>x<height> for the windowed size
// Defaults:
//   ship    -> borderless fullscreen on the primary monitor, vsync on
//   dbg/opt -> 1920x1080 windowed, vsync off
LaunchOptions ParseLaunchOptions(int argc, char** argv);
