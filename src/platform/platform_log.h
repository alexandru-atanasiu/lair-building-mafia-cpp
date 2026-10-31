#pragma once

#include "core/log.h"

// Implemented per platform. Writes one formatted line (no trailing newline) to the
// platform's log sinks. Called with the logger's lock held.
void PlatformLogOutput(LogLevel level, const char* line);
