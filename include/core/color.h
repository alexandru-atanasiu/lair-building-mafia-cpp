#pragma once

#include <algorithm>
#include <cstdint>

struct Color
{
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f; // straight (not premultiplied) alpha
};

// RGBA8 with R in the lowest byte (DXGI R8G8B8A8_UNORM / VertexFormat::UNorm8x4 layout).
// Channels are clamped to [0, 1].
inline uint32_t PackColorRGBA8(const Color& c)
{
    auto channel = [](float v) { return (uint32_t)(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
    return channel(c.r) | (channel(c.g) << 8) | (channel(c.b) << 16) | (channel(c.a) << 24);
}
