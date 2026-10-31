#include "core/file.h"

#include <fstream>

bool ReadFileBytes(const char* path, std::vector<uint8_t>& out)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return false;

    const std::streamsize size = file.tellg();
    if (size < 0)
        return false;

    out.resize((size_t)size);
    file.seekg(0);
    return size == 0 || file.read(reinterpret_cast<char*>(out.data()), size).good();
}
