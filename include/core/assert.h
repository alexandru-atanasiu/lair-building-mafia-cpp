#pragma once

// Assertions, compiled in only when GAME_ASSERTS_ENABLED is defined (dbg/opt builds).
// When disabled the condition is NOT evaluated.
//
//   GAME_ASSERT(cond)
//   GAME_ASSERT(cond, "fmt", args...)          // optional printf-style message (must start with a literal)
//   GAME_ASSERT_CRITICAL(cond)
//   GAME_ASSERT_CRITICAL(cond, "fmt", args...)
//
// On failure the condition, message, location and callstack are logged (LOG_ERR), then:
//
//                          GAME_ASSERT_POPUP                    no popup
//   GAME_ASSERT            dialog: Retry / Continue / Abort     continue
//   GAME_ASSERT_CRITICAL   dialog: Retry / Abort                crash
//
// Retry re-evaluates the condition (e.g. after fixing state in the debugger).
// Abort quits the game.

enum class AssertKind
{
    Normal,
    Critical,
};

enum class AssertAction
{
    Retry,
    Continue,
};

#if defined(GAME_ASSERTS_ENABLED)

// Called when an assertion fails. Returns what the caller should do next;
// doesn't return at all on Abort or on a critical failure without a popup.
AssertAction AssertFailed(AssertKind kind, const char* expr, const char* file, int line, const char* fmt, ...);

// `"" __VA_ARGS__` turns an absent message into "" and leaves `"fmt", args...` unchanged.
#define GAME_ASSERT_IMPL(kind, cond, ...)                                                          \
    do {                                                                                           \
        while (!(cond))                                                                            \
        {                                                                                          \
            if (::AssertFailed(kind, #cond, __FILE__, __LINE__, "" __VA_ARGS__) != ::AssertAction::Retry) \
                break;                                                                             \
        }                                                                                          \
    } while (0)

#define GAME_ASSERT(cond, ...)          GAME_ASSERT_IMPL(::AssertKind::Normal,   cond, __VA_ARGS__)
#define GAME_ASSERT_CRITICAL(cond, ...) GAME_ASSERT_IMPL(::AssertKind::Critical, cond, __VA_ARGS__)

#else

// Declared but never defined: only referenced inside an unevaluated sizeof, so the
// condition and message still compile (and count as used) but generate no code.
int AssertDiscard(const char* fmt, ...);

#define GAME_ASSERT(cond, ...)          ((void)sizeof(!(cond)), (void)sizeof(::AssertDiscard("" __VA_ARGS__)))
#define GAME_ASSERT_CRITICAL(cond, ...) ((void)sizeof(!(cond)), (void)sizeof(::AssertDiscard("" __VA_ARGS__)))

#endif
