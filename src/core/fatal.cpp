#include "core/fatal.h"

#include <cstdio>

void FatalMissingAsset(const char* path)
{
    char msg[1024];
    snprintf(msg, sizeof(msg),
             "Can't load '%s'.\n\n"
             "Run cook.bat, and start the game from the data folder (run.bat does this).",
             path);
    Fatal(msg);
}
