#include "core/fatal.h"

#include "core/log.h"
#include "../platform_debug.h"

#include <windows.h>

#include <cstdio>

void Fatal(const char* msg, long code)
{
    LOG_ERR("Fatal: %s (code 0x%08X)", msg, (unsigned)code);

    char buf[2048];
    if (code != 0)
        snprintf(buf, sizeof(buf), "%s\n\nError code: 0x%08X", msg, (unsigned)code);
    else
        snprintf(buf, sizeof(buf), "%s", msg);

    // A fatal error during mouse-look would otherwise leave the cursor hidden and pinned.
    PlatformReleaseCursor();
    MessageBoxA(nullptr, buf, "Fatal error", MB_OK | MB_ICONERROR | MB_TOPMOST);
    ExitProcess(1);
}
