#pragma once

#include <cstddef>

// Implemented per platform. Writes the full UTF-8 path of an installed system font file
// (e.g. "consola.ttf") into `outPath`. Returns false if the font isn't installed.
bool PlatformFindSystemFont(const char* fileName, char* outPath, size_t outPathSize);
