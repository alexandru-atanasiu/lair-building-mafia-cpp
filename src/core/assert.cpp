#include "core/assert.h"

#if defined(GAME_ASSERTS_ENABLED)

#include "core/log.h"
#include "../platform/platform_debug.h"

#include <cstdarg>
#include <cstdio>

namespace
{
    // Set while handling a failure on this thread, so an assert fired from inside the
    // handler (e.g. from a log sink) can't recurse forever.
    thread_local bool t_handlingAssert = false;
}

AssertAction AssertFailed(AssertKind kind, const char* expr, const char* file, int line, const char* fmt, ...)
{
    const bool critical = kind == AssertKind::Critical;

    if (t_handlingAssert)
    {
        if (critical)
            PlatformCrash();
        return AssertAction::Continue;
    }
    t_handlingAssert = true;

    char message[1024] = "";
    if (fmt && fmt[0])
    {
        va_list args;
        va_start(args, fmt);
        vsnprintf(message, sizeof(message), fmt, args);
        va_end(args);
    }

    char callstack[8192];
    PlatformCaptureCallstack(callstack, sizeof(callstack), 1); // skip AssertFailed itself

    const char* title = critical ? "Critical assertion failed" : "Assertion failed";

#if defined(GAME_LOGGING_ENABLED)
    // Logged with the assert's own file/line rather than this one.
    LogWrite(LogLevel::Error, file, line, "%s: %s\n    Message:  %s\n    Location: %s(%d)\n    Callstack:\n%s",
             title, expr, message[0] ? message : "-", file, line, callstack);
#endif

    AssertAction action = AssertAction::Continue;

#if defined(GAME_ASSERT_POPUP)
    char body[2048];
    snprintf(body, sizeof(body), "Condition:  %s\n%s%s%sFile:  %s\nLine:  %d",
             expr,
             message[0] ? "Message:  " : "", message, message[0] ? "\n" : "",
             file, line);

    switch (PlatformShowAssertDialog(title, body, callstack, !critical))
    {
    case AssertDialogResult::Retry:
        action = AssertAction::Retry;
        break;
    case AssertDialogResult::Continue:
        action = AssertAction::Continue;
        break;
    case AssertDialogResult::Abort:
        LOG_ERR("Aborted from assert dialog");
        PlatformExit(3);
    }
#else
    if (critical)
    {
        LOG_ERR("Critical assertion failed, crashing");
        PlatformCrash();
    }
#endif

    t_handlingAssert = false;
    return action;
}

#endif
