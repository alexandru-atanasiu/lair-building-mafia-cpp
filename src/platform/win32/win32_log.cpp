#include "../platform_log.h"

#if defined(GAME_LOGGING_ENABLED)

#include <windows.h>

#include <cstring>

namespace
{
    // We're a GUI-subsystem app, so there's no console of our own. If stdout was
    // redirected (e.g. `run.bat dbg > log.txt`) write there; otherwise, if we were
    // launched from a console (e.g. via run.bat), attach to it so logs show up there.
    struct ParentConsole
    {
        HANDLE handle        = INVALID_HANDLE_VALUE;
        bool   supportsColor = false;

        ParentConsole()
        {
            HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
            if (stdOut && stdOut != INVALID_HANDLE_VALUE)
            {
                const DWORD type = GetFileType(stdOut);
                if (type == FILE_TYPE_DISK || type == FILE_TYPE_PIPE)
                {
                    handle = stdOut;
                    return;
                }
            }

            if (!AttachConsole(ATTACH_PARENT_PROCESS))
                return;

            handle = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
            DWORD mode = 0;
            if (handle != INVALID_HANDLE_VALUE && GetConsoleMode(handle, &mode))
                supportsColor = SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
        }

        void Write(const char* text) const
        {
            DWORD written = 0;
            WriteFile(handle, text, (DWORD)strlen(text), &written, nullptr);
        }
    };

    const ParentConsole& GetParentConsole()
    {
        static const ParentConsole console;
        return console;
    }

    const char* LevelColor(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Info:  return nullptr;
        case LogLevel::Warn:  return "\x1b[33m"; // yellow
        case LogLevel::Error: return "\x1b[31m"; // red
        }
        return nullptr;
    }
}

void PlatformLogOutput(LogLevel level, const char* line)
{
    OutputDebugStringA(line);
    OutputDebugStringA("\n");

    const ParentConsole& console = GetParentConsole();
    if (console.handle == INVALID_HANDLE_VALUE)
        return;

    const char* color = console.supportsColor ? LevelColor(level) : nullptr;
    if (color)
        console.Write(color);
    console.Write(line);
    if (color)
        console.Write("\x1b[0m");
    console.Write("\n");
}

#endif
