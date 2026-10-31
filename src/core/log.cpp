#include "core/log.h"

#if defined(GAME_LOGGING_ENABLED)

#include "core/path_utils.h"
#include "../platform/platform_log.h"

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace
{
    const auto g_startTime = std::chrono::steady_clock::now();

    struct Listener
    {
        LogListenerFn fn;
        void*         user;

        bool operator==(const Listener&) const = default;
    };

    struct HistoryEntry
    {
        LogLevel    level;
        std::string line;
    };

    struct LogState
    {
        std::mutex               mutex;
        std::vector<Listener>    listeners;
        std::deque<HistoryEntry> history;
    };

    // Function-local static: safe to use from other translation units' static initializers.
    LogState& GetLogState()
    {
        static LogState state;
        return state;
    }

    const char* LevelTag(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERR ";
        }
        return "????";
    }
}

void LogWrite(LogLevel level, const char* file, int line, const char* fmt, ...)
{
    char message[8192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - g_startTime).count();

    char output[8448];
    snprintf(output, sizeof(output), "[%9.3f][%s] %s(%d): %s",
             seconds, LevelTag(level), PathFileName(file), line, message);

    LogState& state = GetLogState();
    std::lock_guard<std::mutex> lock(state.mutex);

    PlatformLogOutput(level, output);

    state.history.push_back({ level, output });
    if (state.history.size() > (size_t)kLogHistorySize)
        state.history.pop_front();

    for (const Listener& listener : state.listeners)
        listener.fn(listener.user, level, output);
}

void LogAddListener(LogListenerFn fn, void* user)
{
    LogState& state = GetLogState();
    std::lock_guard<std::mutex> lock(state.mutex);

    for (const HistoryEntry& entry : state.history)
        fn(user, entry.level, entry.line.c_str());

    state.listeners.push_back({ fn, user });
}

void LogRemoveListener(LogListenerFn fn, void* user)
{
    LogState& state = GetLogState();
    std::lock_guard<std::mutex> lock(state.mutex);

    std::erase(state.listeners, Listener{ fn, user });
}

#endif
