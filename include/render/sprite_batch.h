#pragma once

#include "core/color.h"
#include "core/math.h"

#include <cstdint>
#include <memory>
#include <vector>

class Camera2D;
class IBuffer;
class IPipeline;
class IRenderDevice;
class ITexture;

// Texture region in UVs: (0, 0) is the top-left of the texture, (1, 1) the bottom-right.
struct UvRect
{
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;
};

struct Sprite
{
    Vec2   position;               // world-space center
    Vec2   size     = { 1, 1 };    // world units
    float  rotation = 0.0f;        // radians, counter-clockwise around the center
    UvRect uv;                     // swap u0/u1 (or v0/v1) to flip
    Color  color;                  // tint, multiplied with the texture
};

struct SpriteBatchStats
{
    uint32_t submitted = 0; // Draw calls on scopes
    uint32_t drawn     = 0; // sprites left after culling
    uint32_t drawCalls = 0; // GPU draws (one per run of sprites sharing a texture)
    uint32_t uploads   = 0; // instance buffer refills
};

// Draws textured quads in large instanced batches. Sprites are drawn in submission order
// (later ones on top) with premultiplied alpha blending; consecutive sprites with the same
// texture share a draw call, so group draws by texture when the order allows it.
//
//   {
//       SpriteBatch::Scope sprites = batch.Begin(camera);
//       sprites.Draw(texture, sprite);
//       ...
//   } // flushed here
//
// Scopes must close between IRenderDevice::BeginFrame and EndFrame, one at a time. Textures
// must stay alive until their scope closes. Sprites outside the camera's view are culled.
class SpriteBatch
{
public:
    class Scope
    {
    public:
        ~Scope();
        Scope(const Scope&)            = delete;
        Scope& operator=(const Scope&) = delete;

        void Draw(const ITexture& texture, const Sprite& sprite);

    private:
        friend class SpriteBatch;
        explicit Scope(SpriteBatch& batch) : m_batch(batch) {}

        SpriteBatch& m_batch;
    };

    // Loads the sprite shaders from the data folder (Fatal if they're missing).
    explicit SpriteBatch(IRenderDevice& device);
    ~SpriteBatch();

    SpriteBatch(const SpriteBatch&)            = delete;
    SpriteBatch& operator=(const SpriteBatch&) = delete;

    // Opens a scope drawing through `camera`, which is copied: later changes don't affect it.
    [[nodiscard]] Scope Begin(const Camera2D& camera);

    // Totals since the last ResetStats, over all scopes.
    const SpriteBatchStats& GetStats() const { return m_stats; }
    void ResetStats() { m_stats = {}; }

    // Layout of one sprite in the instance buffer (matches shaders/sprite.vs.hlsl).
    struct Instance
    {
        float    center[2];
        float    halfSize[2];
        float    rotation;
        float    uvRect[4];
        uint32_t color; // RGBA8, straight alpha
    };

private:
    void Add(const ITexture& texture, const Sprite& sprite);
    void Flush();
    void EnsureCapacity(uint32_t instanceCount);

    IRenderDevice&             m_device;
    std::unique_ptr<IPipeline> m_pipeline;
    std::unique_ptr<IBuffer>   m_cameraConstants;
    std::unique_ptr<IBuffer>   m_instanceBuffer;
    uint32_t                   m_capacity = 0; // instances m_instanceBuffer holds

    // Sprites of the open scope, in submission order (parallel arrays).
    std::vector<Instance>        m_instances;
    std::vector<const ITexture*> m_textures;

    bool             m_inScope = false;
    Vec2             m_viewMin; // culling rectangle of the open scope
    Vec2             m_viewMax;
    Vec2             m_worldToClipScale;
    Vec2             m_worldToClipOffset;
    SpriteBatchStats m_stats;
};
