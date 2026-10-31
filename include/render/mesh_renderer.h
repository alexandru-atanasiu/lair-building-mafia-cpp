#pragma once

#include "core/color.h"
#include "core/math.h"
#include "render/camera_3d.h"
#include "render/lighting.h"

#include <cstdint>
#include <memory>

class IBuffer;
class IPipeline;
class IRenderDevice;
class Mesh;
template <typename MeshT, typename InstanceT>
class InstanceBuckets;

struct MeshRendererStats
{
    uint32_t submitted = 0; // Draw calls on scopes
    uint32_t drawn     = 0; // instances left after frustum culling
    uint32_t drawCalls = 0; // GPU draws (one per mesh per upload)
    uint32_t uploads   = 0; // instance buffer refills
};

// Draws static meshes with hardware instancing, flat colored and Gouraud shaded (lighting is
// evaluated per vertex and interpolated) by one point light plus ambient.
//
//   {
//       MeshRenderer::Scope meshes = renderer.Begin(camera, lighting);
//       meshes.Draw(mesh, worldMatrix, color);
//       ...
//   } // flushed here
//
// Draws are opaque and depth tested, so they're regrouped by mesh: one draw call per distinct
// mesh, whatever the submission order. Instances outside the view frustum are culled.
// Scopes must close between IRenderDevice::BeginFrame and EndFrame, one at a time; meshes must
// stay alive until their scope closes.
class MeshRenderer
{
public:
    class Scope
    {
    public:
        ~Scope();
        Scope(const Scope&)            = delete;
        Scope& operator=(const Scope&) = delete;

        // `world` may contain any affine transform (non-uniform scale and mirroring included).
        // `emissive` blends from lit (0) to the unlit color (1), e.g. for light markers.
        void Draw(const Mesh& mesh, const Mat4& world, const Color& color, float emissive = 0.0f);

    private:
        friend class MeshRenderer;
        explicit Scope(MeshRenderer& renderer) : m_renderer(renderer) {}

        MeshRenderer& m_renderer;
    };

    // Loads the mesh shaders from the data folder (Fatal if they're missing).
    explicit MeshRenderer(IRenderDevice& device);
    ~MeshRenderer();

    MeshRenderer(const MeshRenderer&)            = delete;
    MeshRenderer& operator=(const MeshRenderer&) = delete;

    // The camera and lighting are copied: later changes don't affect the open scope.
    [[nodiscard]] Scope Begin(const Camera3D& camera, const MeshLighting& lighting);

    // Totals since the last ResetStats, over all scopes.
    const MeshRendererStats& GetStats() const { return m_stats; }
    void ResetStats() { m_stats = {}; }

    // Layout of one instance in the instance buffer (matches shaders/mesh.vs.hlsl).
    struct Instance
    {
        float    world[3][4]; // top 3 rows of the world matrix
        uint32_t color;       // RGBA8
        float    emissive;
    };

private:
    void Add(const Mesh& mesh, const Mat4& world, const Color& color, float emissive);
    void Flush();

    IRenderDevice&                                   m_device;
    std::unique_ptr<IPipeline>                       m_pipeline;
    std::unique_ptr<IBuffer>                         m_frameConstants;
    std::unique_ptr<InstanceBuckets<Mesh, Instance>> m_instances;

    bool              m_inScope = false;
    Mat4              m_viewProj;
    Vec3              m_cameraPosition;
    Frustum           m_frustum;
    MeshLighting      m_lighting;
    MeshRendererStats m_stats;
};
