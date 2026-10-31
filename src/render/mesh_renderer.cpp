#include "render/mesh_renderer.h"

#include "core/assert.h"
#include "render/mesh.h"
#include "render/render_device.h"
#include "render/shader_file.h"

#include "instance_buckets.h"
#include "lighting_constants.h"

#include <cstddef>

namespace
{
    // Cooked by cook.bat from shaders/, relative to the data folder (the working directory).
    constexpr const char* kMeshVS = "shaders/mesh.vs.cso";
    constexpr const char* kMeshPS = "shaders/mesh.ps.cso";

    // The instance buffer grows to fit a scope's instances up to this many; bigger scopes are
    // uploaded and drawn in several chunks.
    constexpr uint32_t kMinInstances = 256;
    constexpr uint32_t kMaxInstances = 64 * 1024;

    using Instance = MeshRenderer::Instance;
    const VertexAttribute kAttributes[] = {
        { "POSITION", VertexFormat::Float3,   offsetof(MeshVertex, position), 0 },
        { "NORMAL",   VertexFormat::Float3,   offsetof(MeshVertex, normal),   0 },
        { "WORLDX",   VertexFormat::Float4,   offsetof(Instance, world[0]),   1 },
        { "WORLDY",   VertexFormat::Float4,   offsetof(Instance, world[1]),   1 },
        { "WORLDZ",   VertexFormat::Float4,   offsetof(Instance, world[2]),   1 },
        { "COLOR",    VertexFormat::UNorm8x4, offsetof(Instance, color),      1 },
        { "EMISSIVE", VertexFormat::Float1,   offsetof(Instance, emissive),   1 },
    };
}

MeshRenderer::Scope::~Scope()
{
    m_renderer.Flush();
}

void MeshRenderer::Scope::Draw(const Mesh& mesh, const Mat4& world, const Color& color, float emissive)
{
    m_renderer.Add(mesh, world, color, emissive);
}

MeshRenderer::MeshRenderer(IRenderDevice& device, MeshRenderStyle style)
    : m_device(device)
    , m_instances(std::make_unique<InstanceBuckets<Mesh, Instance>>(device, kMinInstances, kMaxInstances))
{
    const std::vector<uint8_t> vs = LoadShaderBytecode(kMeshVS);
    const std::vector<uint8_t> ps = LoadShaderBytecode(kMeshPS);

    // Stream 0: mesh vertices. Stream 1: one Instance per drawn object.
    PipelineDesc pd   = {};
    pd.vertexShader   = { vs.data(), vs.size() };
    pd.pixelShader    = { ps.data(), ps.size() };
    pd.attributes     = kAttributes;
    pd.attributeCount = (uint32_t)(sizeof(kAttributes) / sizeof(kAttributes[0]));
    pd.vertexStride   = sizeof(MeshVertex);
    pd.instanceStride = sizeof(Instance);
    pd.topology       = PrimitiveTopology::TriangleList;
    pd.blend          = style == MeshRenderStyle::Translucent ? BlendMode::PremultipliedAlpha : BlendMode::Opaque;
    pd.cull           = CullMode::Back;
    pd.depth          = style == MeshRenderStyle::Translucent ? DepthMode::ReadOnly : DepthMode::ReadWrite;

    m_pipeline       = m_device.CreatePipeline(pd);
    m_frameConstants = m_device.CreateConstantBuffer(sizeof(LightingConstants));
}

MeshRenderer::~MeshRenderer() = default;

MeshRenderer::Scope MeshRenderer::Begin(const Camera3D& camera, const MeshLighting& lighting)
{
    GAME_ASSERT_CRITICAL(!m_inScope, "MeshRenderer::Begin while another scope is open");
    m_inScope = true;

    m_viewProj       = camera.GetViewProjection();
    m_cameraPosition = camera.GetPosition();
    m_frustum        = Frustum::FromViewProjection(m_viewProj);
    m_lighting       = lighting;
    return Scope(*this);
}

void MeshRenderer::Add(const Mesh& mesh, const Mat4& world, const Color& color, float emissive)
{
    ++m_stats.submitted;

    const Vec3  center = world.TransformPoint(mesh.GetBoundsCenter());
    const float radius = mesh.GetBoundsRadius() * world.MaxAxisScale();
    if (!m_frustum.IntersectsSphere(center, radius))
        return;

    Instance& inst = m_instances->Add(mesh);
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 4; ++col)
            inst.world[row][col] = world.m[row][col];
    inst.color    = PackColorRGBA8(color);
    inst.emissive = emissive;
}

void MeshRenderer::Flush()
{
    GAME_ASSERT(m_inScope, "MeshRenderer flushed without an open scope");
    m_inScope = false;

    if (m_instances->GetCount() > 0)
    {
        const LightingConstants constants = MakeLightingConstants(m_viewProj, m_cameraPosition, m_lighting);
        m_device.UpdateBuffer(*m_frameConstants, &constants, sizeof(constants));
    }

    DrawDesc draw;
    draw.pipeline       = m_pipeline.get();
    draw.constantBuffer = m_frameConstants.get();

    const InstanceBucketsFlushStats flushed = m_instances->Flush(draw);
    m_stats.drawn     += flushed.instances;
    m_stats.uploads   += flushed.uploads;
    m_stats.drawCalls += flushed.drawCalls;
}
