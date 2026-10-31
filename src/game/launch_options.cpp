#include "game/launch_options.h"

#include "core/log.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if !defined(GAME_DBG) && !defined(GAME_OPT) && !defined(GAME_SHIP)
#error "One of GAME_DBG, GAME_OPT, GAME_SHIP must be defined"
#endif

LaunchOptions ParseLaunchOptions(int argc, char** argv)
{
    LaunchOptions options;
    WindowDesc&       window = options.window;
    RenderDeviceDesc& render = options.renderDevice;

    window.width  = 1920;
    window.height = 1080;

#if defined(GAME_DBG)
    window.title = "Capo di Tutti Capi [dbg]";
    window.mode  = WindowMode::Windowed;
    render.vsync = false;
#elif defined(GAME_OPT)
    window.title = "Capo di Tutti Capi [opt]";
    window.mode  = WindowMode::Windowed;
    render.vsync = false;
#else
    window.title = "Capo di Tutti Capi";
    window.mode  = WindowMode::BorderlessFullscreen;
    render.vsync = true;
#endif

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-windowed") == 0)
            window.mode = WindowMode::Windowed;
        else if (strcmp(argv[i], "-fullscreen") == 0)
            window.mode = WindowMode::BorderlessFullscreen;
        else if (strcmp(argv[i], "-vsync") == 0)
            render.vsync = true;
        else if (strcmp(argv[i], "-novsync") == 0)
            render.vsync = false;
#if defined(GAME_DEV_TOOLS_ENABLED)
        else if (strcmp(argv[i], "-view") == 0 && i + 1 < argc)
            options.dev.startView = argv[++i];
        else if (strcmp(argv[i], "-pipe") == 0 && i + 1 < argc)
            options.dev.pipeName = argv[++i];
        else if (strcmp(argv[i], "-script") == 0 && i + 1 < argc)
            options.dev.scriptPath = argv[++i];
        else if (strcmp(argv[i], "-run") == 0 && i + 1 < argc)
            options.dev.commands = argv[++i];
        else if (strcmp(argv[i], "-nodevui") == 0)
            options.dev.hideDevUi = true;
        else if (strcmp(argv[i], "-dpiscale") == 0 && i + 1 < argc)
            options.dev.dpiScale = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "-size") == 0 && i + 1 < argc)
        {
            unsigned w = 0, h = 0;
            if (sscanf_s(argv[++i], "%ux%u", &w, &h) == 2)
            {
                window.width  = w;
                window.height = h;
            }
            else
                LOG_WARN("-size expects WIDTHxHEIGHT, got '%s'", argv[i]);
        }
#endif
        else
            LOG_WARN("Unknown command-line argument '%s' (see launch_options.h)", argv[i]);
    }

    return options;
}
