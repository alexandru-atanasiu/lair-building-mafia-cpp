#include "render/sprite_batch.h"

#include "core/assert.h"
#include "render/camera_2d.h"
#include "render/render_device.h"
#include "render/shader_file.h"

// SpriteCameraConstants: cbuffer of shaders/sprite.vs.hlsl, declared once for HLSL and C++.
#include "../../shaders/sprite_cb.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>

namespace
{
    // Cooked by cook.bat from shaders/, relative to the data folder (the working directory).
    constexpr const char* kSpriteVS = "shaders/sprite.vs.cso";
    constexpr const char* kSpritePS = "shaders/sprite.ps.cso";

    // The instance buffer grows to fit a scope's sprites up to this many; bigger scopes are
    // uploaded and drawn in several chunks.
    constexpr uint32_t kMinInstances = 1024;
    constexpr uint32_t kMaxInstances = 64 * 1024;

    static_assert(sizeof(SpriteCameraConstants) % 16 == 0);

    using Instance = SpriteBatch::Instance;
    const VertexAttribute kInstanceAttributes[] = {
        { "CENTER",   VertexFormat::Float2,   offsetof(Instance, center)   },
        { "HALFSIZE", VertexFormat::Float2,   offsetof(Instance, halfSize) },
        { "ROTATION", VertexFormat::Float1,   offsetof(Instance, rotation) },
        { "UVRECT",   VertexFormat::Float4,   offsetof(Instance, uvRect)   },
        { "COLOR",    VertexFormat::UNorm8x4, offsetof(Instance, color)    },
    };
}

SpriteBatch::Scope::~Scope()
{
    m_batch.Flush();
}

void SpriteBatch::Scope::Draw(const ITexture& texture, const Sprite& sprite)
{
    m_batch.Add(texture, sprite);
}

SpriteBatch::SpriteBatch(IRenderDevice& device)
    : m_device(device)
{
    const std::vector<uint8_t> vs = LoadShaderBytecode(kSpriteVS);
    const std::vector<uint8_t> ps = LoadShaderBytecode(kSpritePS);

    // No per-vertex data: each instance is one sprite, expanded to a 4-vertex strip
    // from SV_VertexID in the vertex shader.
    PipelineDesc pd   = {};
    pd.vertexShader   = { vs.data(), vs.size() };
    pd.pixelShader    = { ps.data(), ps.size() };
    pd.attributes     = kInstanceAttributes;
    pd.attributeCount = (uint32_t)(sizeof(kInstanceAttributes) / sizeof(kInstanceAttributes[0]));
    pd.vertexStride   = sizeof(Instance);
    pd.vertexRate     = VertexRate::PerInstance;
    pd.topology       = PrimitiveTopology::TriangleStrip;
    pd.blend          = BlendMode::PremultipliedAlpha;
    pd.cull           = CullMode::None; // mirrored sprites flip the winding
    pd.filter         = TextureFilter::Linear;

    m_pipeline        = m_device.CreatePipeline(pd);
    m_cameraConstants = m_device.CreateConstantBuffer(sizeof(SpriteCameraConstants));
}

SpriteBatch::~SpriteBatch() = default;

SpriteBatch::Scope SpriteBatch::Begin(const Camera2D& camera)
{
    GAME_ASSERT_CRITICAL(!m_inScope, "SpriteBatch::Begin while another scope is open");
    m_inScope = true;

    camera.GetVisibleBounds(m_viewMin, m_viewMax);
    camera.GetWorldToClip(m_worldToClipScale, m_worldToClipOffset);
    return Scope(*this);
}

void SpriteBatch::Add(const ITexture& texture, const Sprite& sprite)
{
    ++m_stats.submitted;

    // Cull against the view rectangle using the sprite's bounding box (or, when rotated,
    // the box around its bounding circle).
    Vec2 half = { std::fabs(sprite.size.x) * 0.5f, std::fabs(sprite.size.y) * 0.5f };
    if (sprite.rotation != 0.0f)
    {
        const float radius = std::sqrt(half.x * half.x + half.y * half.y);
        half = { radius, radius };
    }
    if (sprite.position.x + half.x < m_viewMin.x || sprite.position.x - half.x > m_viewMax.x ||
        sprite.position.y + half.y < m_viewMin.y || sprite.position.y - half.y > m_viewMax.y)
        return;

    Instance& inst   = m_instances.emplace_back();
    inst.center[0]   = sprite.position.x;
    inst.center[1]   = sprite.position.y;
    inst.halfSize[0] = sprite.size.x * 0.5f;
    inst.halfSize[1] = sprite.size.y * 0.5f;
    inst.rotation    = sprite.rotation;
    inst.uvRect[0]   = sprite.uv.u0;
    inst.uvRect[1]   = sprite.uv.v0;
    inst.uvRect[2]   = sprite.uv.u1;
    inst.uvRect[3]   = sprite.uv.v1;
    inst.color       = PackColorRGBA8(sprite.color);
    m_textures.push_back(&texture);
}

void SpriteBatch::EnsureCapacity(uint32_t instanceCount)
{
    if (instanceCount <= m_capacity)
        return;

    m_capacity       = std::clamp(std::bit_ceil(instanceCount), kMinInstances, kMaxInstances);
    m_instanceBuffer = m_device.CreateDynamicVertexBuffer(m_capacity * (uint32_t)sizeof(Instance));
}

void SpriteBatch::Flush()
{
    GAME_ASSERT(m_inScope, "SpriteBatch flushed without an open scope");
    m_inScope = false;

    const uint32_t count = (uint32_t)m_instances.size();
    if (count == 0)
        return;
    m_stats.drawn += count;

    const SpriteCameraConstants constants = {
        { m_worldToClipScale.x, m_worldToClipScale.y },
        { m_worldToClipOffset.x, m_worldToClipOffset.y },
    };
    m_device.UpdateBuffer(*m_cameraConstants, &constants, sizeof(constants));

    EnsureCapacity(std::min(count, kMaxInstances));

    DrawDesc draw;
    draw.pipeline       = m_pipeline.get();
    draw.vertexBuffer   = m_instanceBuffer.get();
    draw.constantBuffer = m_cameraConstants.get();
    draw.vertexCount    = 4;

    for (uint32_t chunkStart = 0; chunkStart < count; chunkStart += m_capacity)
    {
        const uint32_t chunkCount = std::min(m_capacity, count - chunkStart);
        m_device.UpdateBuffer(*m_instanceBuffer, &m_instances[chunkStart], chunkCount * (uint32_t)sizeof(Instance));
        ++m_stats.uploads;

        // One draw per run of consecutive sprites with the same texture.
        const ITexture* const* textures = &m_textures[chunkStart];
        for (uint32_t runStart = 0; runStart < chunkCount;)
        {
            uint32_t runEnd = runStart + 1;
            while (runEnd < chunkCount && textures[runEnd] == textures[runStart])
                ++runEnd;

            draw.texture       = textures[runStart];
            draw.firstInstance = runStart;
            draw.instanceCount = runEnd - runStart;
            m_device.Draw(draw);
            ++m_stats.drawCalls;

            runStart = runEnd;
        }
    }

    m_instances.clear();
    m_textures.clear();
}
