#include "render/shader_file.h"

#include "core/fatal.h"
#include "core/file.h"

std::vector<uint8_t> LoadShaderBytecode(const char* path)
{
    std::vector<uint8_t> bytecode;
    if (!ReadFileBytes(path, bytecode) || bytecode.empty())
        FatalMissingAsset(path);
    return bytecode;
}
