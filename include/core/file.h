#pragma once

#include <cstdint>
#include <vector>

// Reads a whole file into `out`. Relative paths are resolved from the working directory,
// which is the data folder at runtime. Returns false if the file can't be read.
bool ReadFileBytes(const char* path, std::vector<uint8_t>& out);
