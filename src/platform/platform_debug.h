#pragma once

#include <cstddef>

// Implemented per platform.

// Writes a human-readable callstack (one indented frame per line) into `buffer`.
// `skipFrames` frames above the caller of this function are omitted.
void PlatformCaptureCallstack(char* buffer, size_t bufferSize, int skipFrames);

enum class AssertDialogResult
{
    Retry,
    Continue,
    Abort,
};

// Shows a modal assert dialog. Continue is only offered when `allowContinue` is true.
AssertDialogResult PlatformShowAssertDialog(const char* title, const char* body, const char* callstack,
                                            bool allowContinue);

// Undoes a mouse-look capture (cursor clip, hidden cursor, mouse capture) so a modal dialog
// shown from anywhere (asserts, fatal errors, crashes) can be used. The window re-captures
// on the next mouse-look.
void PlatformReleaseCursor();

// Installs a handler for unhandled exceptions (crashes) that writes a minidump and tells the
// user where it is. Call once, at startup.
void PlatformInstallCrashHandler();

// Terminates the process immediately with a crash (debugger / crash reporting gets a shot at it).
[[noreturn]] void PlatformCrash();

// Terminates the process normally with the given exit code.
[[noreturn]] void PlatformExit(int exitCode);
