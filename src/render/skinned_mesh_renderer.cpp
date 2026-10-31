#include "render/skinned_mesh_renderer.h"

#include "core/assert.h"
#include "render/render_device.h"
#include "render/shader_file.h"
#include "render/skinned_mesh.h"

#include "instance_buckets.h"
#include "lighting_constants.h"

#include <algorithm>
#include <bit>
#include <cstddef>

namespace
{
    // Cooked by cook.bat from shaders/, relative to the data folder (the working directory).
    constexpr const char* kSkinnedVS = "shaders/skinned.vs.cso";
    constexpr const char* kSkinnedPS = "shaders/skinned.ps.cso";

    // Instance buffer: grows to fit a scope up to this many instances, bigger scopes are drawn
    // in chunks. The palette always holds the whole scope.
    constexpr uint32_t kMinInstances = 64;
    constexpr uint32_t kMaxInstances = 64 * 1024;
    constexpr uint32_t kMinPalette   = 1024;

    using Instance = SkinnedMeshRenderer::Instance;
    const VertexAttribute kAttributes[] = {
        { "POSITION", VertexFormat::Float3,   offsetof(SkinnedVertex, position),  0 },
        { "NORMAL",   VertexFormat::Float3,   offsetof(SkinnedVertex, normal),    0 },
        { "JOINTS",   VertexFormat::UInt8x4,  offsetof(SkinnedVertex, joints),    0 },
        { "WEIGHTS",  VertexFormat::Float4,   offsetof(SkinnedVertex, weights),   0 },
        { "MATERIAL", VertexFormat::UNorm8x4, offsetof(SkinnedVertex, color),     0 },
        { "PALETTE",  VertexFormat::UInt1,    offsetof(Instance, paletteOffset),  1 },
        { "COLOR",    VertexFormat::UNorm8x4, offsetof(Instance, color),          1 },
    };
}

SkinnedMeshRenderer::Scope::~Scope()
{
    m_renderer.Flush();
}

void SkinnedMeshRenderer::Scope::Draw(const SkinnedMesh& mesh, const Mat4& world, const Mat4* modelMatrices, const Color& color)
{
    m_renderer.Add(mesh, world, modelMatrices, color);
}

SkinnedMeshRenderer::SkinnedMeshRenderer(IRenderDevice& device)
    : m_device(device)
    , m_instances(std::make_unique<InstanceBuckets<SkinnedMesh, Instance>>(device, kMinInstances, kMaxInstances))
{
    const std::vector<uint8_t> vs = LoadShaderBytecode(kSkinnedVS);
    const std::vector<uint8_t> ps = LoadShaderBytecode(kSkinnedPS);

    // Stream 0: skinned mesh vertices. Stream 1: one Instance per drawn character.
    PipelineDesc pd   = {};
    pd.vertexShader   = { vs.data(), vs.size() };
    pd.pixelShader    = { ps.data(), ps.size() };
    pd.attributes     = kAttributes;
    pd.attributeCount = (uint32_t)(sizeof(kAttributes) / sizeof(kAttributes[0]));
    pd.vertexStride   = sizeof(SkinnedVertex);
    pd.instanceStride = sizeof(Instance);
    pd.topology       = PrimitiveTopology::TriangleList;
    pd.blend          = BlendMode::Opaque;
    pd.cull           = CullMode::Back;
    pd.depth          = DepthMode::ReadWrite;

    m_pipeline       = m_device.CreatePipeline(pd);
    m_frameConstants = m_device.CreateConstantBuffer(sizeof(LightingConstants));
}

SkinnedMeshRenderer::~SkinnedMeshRenderer() = default;

SkinnedMeshRenderer::Scope SkinnedMeshRenderer::Begin(const Camera3D& camera, const MeshLighting& lighting)
{
    GAME_ASSERT_CRITICAL(!m_inScope, "SkinnedMeshRenderer::Begin while another scope is open");
    m_inScope = true;

    m_viewProj       = camera.GetViewProjection();
    m_cameraPosition = camera.GetPosition();
    m_frustum        = Frustum::FromViewProjection(m_viewProj);
    m_lighting       = lighting;
    m_palette.clear();
    return Scope(*this);
}

void SkinnedMeshRenderer::Add(const SkinnedMesh& mesh, const Mat4& world, const Mat4* modelMatrices, const Color& color)
{
    ++m_stats.submitted;
    GAME_ASSERT_CRITICAL(modelMatrices, "Skinned draw without joint matrices");

    const Vec3  center = world.TransformPoint(mesh.GetBoundsCenter());
    const float radius = mesh.GetBoundsRadius() * kCullingBoundsScale * world.MaxAxisScale();
    if (!m_frustum.IntersectsSphere(center, radius))
        return;

    Instance& inst     = m_instances->Add(mesh);
    inst.paletteOffset = (uint32_t)m_palette.size();
    inst.color         = PackColorRGBA8(color);

    // Skinning matrix: bind-pose vertex -> joint's animated model space -> world. All three
    // are affine, so only the top 3 rows are computed.
    const Mat4*    inverseBinds = mesh.GetInverseBinds();
    const uint32_t jointCount   = mesh.GetJointCount();
    for (uint32_t j = 0; j < jointCount; ++j)
    {
        const Mat4   skin = MulAffine(MulAffine(world, modelMatrices[j]), inverseBinds[j]);
        JointMatrix& out  = m_palette.emplace_back();
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 4; ++col)
                out.rows[row][col] = skin.m[row][col];
    }
}

void SkinnedMeshRenderer::Flush()
{
    GAME_ASSERT(m_inScope, "SkinnedMeshRenderer flushed without an open scope");
    m_inScope = false;

    if (m_instances->GetCount() > 0)
    {
        const LightingConstants constants = MakeLightingConstants(m_viewProj, m_cameraPosition, m_lighting);
        m_device.UpdateBuffer(*m_frameConstants, &constants, sizeof(constants));

        // Palette: the whole scope in one structured buffer.
        const uint32_t paletteSize = (uint32_t)m_palette.size();
        if (paletteSize > m_paletteCapacity)
        {
            m_paletteCapacity = std::max(std::bit_ceil(paletteSize), kMinPalette);
            m_paletteBuffer   = m_device.CreateDynamicStructuredBuffer(sizeof(JointMatrix), m_paletteCapacity);
        }
        if (paletteSize > 0)
            m_device.UpdateBuffer(*m_paletteBuffer, m_palette.data(), paletteSize * (uint32_t)sizeof(JointMatrix));
        m_stats.joints += paletteSize;
    }

    DrawDesc draw;
    draw.pipeline           = m_pipeline.get();
    draw.constantBuffer     = m_frameConstants.get();
    draw.vertexShaderBuffer = m_paletteBuffer.get();

    const InstanceBucketsFlushStats flushed = m_instances->Flush(draw);
    m_stats.drawn     += flushed.instances;
    m_stats.drawCalls += flushed.drawCalls;
}
