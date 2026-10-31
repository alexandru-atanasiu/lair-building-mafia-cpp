#pragma once

// printf-style logging, compiled in only when GAME_LOGGING_ENABLED is defined (dbg/opt builds).
//
//   LOG_INFO("Window created: %ux%u", width, height);
//   LOG_WARN("Falling back to %s", name);
//   LOG_ERR("CreateBuffer failed: 0x%08X", hr);

enum class LogLevel
{
    Info,
    Warn,
    Error,
};

#if defined(GAME_LOGGING_ENABLED)

void LogWrite(LogLevel level, const char* file, int line, const char* fmt, ...);

#define LOG_INFO(...) ::LogWrite(::LogLevel::Info,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...) ::LogWrite(::LogLevel::Warn,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERR(...)  ::LogWrite(::LogLevel::Error, __FILE__, __LINE__, __VA_ARGS__)

// Receives every formatted log line (no trailing newline; may contain inner newlines).
// Called from whichever thread logged, with the log lock held: must not log itself.
using LogListenerFn = void (*)(void* user, LogLevel level, const char* line);

// Registers a listener. It is immediately sent the recent history (the last
// kLogHistorySize lines), so listeners created after startup still see startup logs.
inline constexpr int kLogHistorySize = 1024;
void LogAddListener(LogListenerFn fn, void* user);
void LogRemoveListener(LogListenerFn fn, void* user);

#else

// Declared but never defined: only referenced inside an unevaluated sizeof, so the
// arguments still compile (and count as used, avoiding unused-variable warnings)
// while generating no code at all.
int LogDiscard(const char* fmt, ...);

#define LOG_INFO(...) ((void)sizeof(::LogDiscard(__VA_ARGS__)))
#define LOG_WARN(...) ((void)sizeof(::LogDiscard(__VA_ARGS__)))
#define LOG_ERR(...)  ((void)sizeof(::LogDiscard(__VA_ARGS__)))

#endif
