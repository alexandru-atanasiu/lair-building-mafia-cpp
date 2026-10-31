#pragma once

#include <cstdint>
#include <vector>

// Loads a compiled shader (.cso, cooked by cook.bat from shaders/), path relative to the data
// folder. Shaders are required: a missing or empty file is a fatal error (FatalMissingAsset).
std::vector<uint8_t> LoadShaderBytecode(const char* path);
