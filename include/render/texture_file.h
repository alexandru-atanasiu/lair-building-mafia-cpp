#pragma once

#include <cstdint>
#include <memory>

class IRenderDevice;
class ITexture;

// Cooked texture file (.tex), written by py/pack_texture.py. Little endian:
//   TextureFileHeader, then `mipCount` levels, largest first. Level i is
//   max(1, width >> i) x max(1, height >> i) pixels, rows top to bottom, tightly packed.
struct TextureFileHeader
{
    char     magic[4]; // "DXTX"
    uint32_t version;  // kTextureFileVersion
    uint32_t format;   // TextureFileFormat
    uint32_t width;
    uint32_t height;
    uint32_t mipCount;
};
static_assert(sizeof(TextureFileHeader) == 24);

constexpr uint32_t kTextureFileVersion = 1;

enum class TextureFileFormat : uint32_t
{
    RGBA8UnormPremultiplied = 1, // 4 bytes per pixel, alpha premultiplied into RGB
};

// Loads a cooked .tex file (path relative to the data folder). Logs and returns null on failure.
std::unique_ptr<ITexture> LoadTexture(IRenderDevice& device, const char* path);
