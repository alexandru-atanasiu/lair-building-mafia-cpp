#include "render/texture_file.h"

#include "core/file.h"
#include "core/log.h"
#include "render/render_device.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace
{
    constexpr uint32_t kMaxTextureSize = 16384;

    uint32_t FullMipCount(uint32_t width, uint32_t height)
    {
        uint32_t count = 1;
        for (uint32_t size = std::max(width, height); size > 1; size >>= 1)
            ++count;
        return count;
    }
}

std::unique_ptr<ITexture> LoadTexture(IRenderDevice& device, const char* path)
{
    std::vector<uint8_t> bytes;
    if (!ReadFileBytes(path, bytes))
    {
        LOG_ERR("Can't read texture '%s'", path);
        return nullptr;
    }

    TextureFileHeader header;
    if (bytes.size() < sizeof(header))
    {
        LOG_ERR("Texture '%s' is truncated (%zu bytes)", path, bytes.size());
        return nullptr;
    }
    memcpy(&header, bytes.data(), sizeof(header));

    if (memcmp(header.magic, "DXTX", 4) != 0 || header.version != kTextureFileVersion)
    {
        LOG_ERR("Texture '%s' isn't a version %u .tex file; re-run cook.bat", path, kTextureFileVersion);
        return nullptr;
    }
    if (header.format != (uint32_t)TextureFileFormat::RGBA8UnormPremultiplied)
    {
        LOG_ERR("Texture '%s' has unknown format %u", path, header.format);
        return nullptr;
    }
    if (header.width == 0 || header.height == 0 || header.width > kMaxTextureSize || header.height > kMaxTextureSize ||
        header.mipCount == 0 || header.mipCount > FullMipCount(header.width, header.height))
    {
        LOG_ERR("Texture '%s' has invalid size %ux%u / %u mips", path, header.width, header.height, header.mipCount);
        return nullptr;
    }

    size_t dataSize = 0;
    for (uint32_t i = 0; i < header.mipCount; ++i)
        dataSize += (size_t)std::max(header.width >> i, 1u) * std::max(header.height >> i, 1u) * 4;
    if (bytes.size() != sizeof(header) + dataSize)
    {
        LOG_ERR("Texture '%s' is %zu bytes, expected %zu", path, bytes.size(), sizeof(header) + dataSize);
        return nullptr;
    }

    TextureDesc desc;
    desc.width    = header.width;
    desc.height   = header.height;
    desc.mipCount = header.mipCount;
    desc.format   = TextureFormat::RGBA8Unorm;
    desc.data     = bytes.data() + sizeof(header);

    LOG_INFO("Texture loaded: %s (%ux%u, %u mips)", path, header.width, header.height, header.mipCount);
    return device.CreateTexture(desc);
}
