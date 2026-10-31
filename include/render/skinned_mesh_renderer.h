#pragma once

#include "core/color.h"
#include "core/math.h"
#include "render/camera_3d.h"
#include "render/lighting.h"

#include <cstdint>
#include <memory>
#include <vector>

class IBuffer;
class IPipeline;
class IRenderDevice;
class SkinnedMesh;
template <typename MeshT, typename InstanceT>
class InstanceBuckets;

struct SkinnedMeshRendererStats
{
    uint32_t submitted = 0; // Draw calls on scopes
    uint32_t drawn     = 0; // instances left after frustum culling
    uint32_t joints    = 0; // skinning matrices uploaded
    uint32_t drawCalls = 0; // GPU draws (one per mesh per upload)
};

// Draws skinned meshes with GPU skinning (up to 4 joint influences per vertex) and hardware
// instancing, Gouraud shaded like MeshRenderer. The color is the mesh's vertex (material) color
// times the instance color.
//
//   {
//       SkinnedMeshRenderer::Scope skinned = renderer.Begin(camera, lighting);
//       skinned.Draw(mesh, worldMatrix, animator.GetModelMatrices(), color);
//   } // flushed here
//
// Each Draw adds the instance's skinning matrices (world * model pose * inverse bind) to a
// palette shared by the scope; instances are regrouped by mesh, one draw call per mesh.
// Scopes must close between IRenderDevice::BeginFrame and EndFrame, one at a time; meshes must
// stay alive until their scope closes.
class SkinnedMeshRenderer
{
public:
    // Animated poses can leave the bind pose's bounds: culling uses them scaled by this.
    static constexpr float kCullingBoundsScale = 1.5f;

    class Scope
    {
    public:
        ~Scope();
        Scope(const Scope&)            = delete;
        Scope& operator=(const Scope&) = delete;

        // `modelMatrices`: one per mesh joint (SkinnedMesh::GetJointCount()), in the skeleton's
        // root space, e.g. Animator::GetModelMatrices(). Copied: may change after the call.
        void Draw(const SkinnedMesh& mesh, const Mat4& world, const Mat4* modelMatrices, const Color& color);

    private:
        friend class SkinnedMeshRenderer;
        explicit Scope(SkinnedMeshRenderer& renderer) : m_renderer(renderer) {}

        SkinnedMeshRenderer& m_renderer;
    };

    // Loads the skinned mesh shaders from the data folder (Fatal if they're missing).
    explicit SkinnedMeshRenderer(IRenderDevice& device);
    ~SkinnedMeshRenderer();

    SkinnedMeshRenderer(const SkinnedMeshRenderer&)            = delete;
    SkinnedMeshRenderer& operator=(const SkinnedMeshRenderer&) = delete;

    // The camera and lighting are copied: later changes don't affect the open scope.
    [[nodiscard]] Scope Begin(const Camera3D& camera, const MeshLighting& lighting);

    // Totals since the last ResetStats, over all scopes.
    const SkinnedMeshRendererStats& GetStats() const { return m_stats; }
    void ResetStats() { m_stats = {}; }

    // Layout of one instance in the instance buffer (matches shaders/skinned.vs.hlsl).
    struct Instance
    {
        uint32_t paletteOffset; // first skinning matrix of the instance
        uint32_t color;         // RGBA8
    };

    // One skinning matrix in the palette: top 3 rows.
    struct JointMatrix
    {
        float rows[3][4];
    };

private:
    void Add(const SkinnedMesh& mesh, const Mat4& world, const Mat4* modelMatrices, const Color& color);
    void Flush();

    IRenderDevice&                                          m_device;
    std::unique_ptr<IPipeline>                              m_pipeline;
    std::unique_ptr<IBuffer>                                m_frameConstants;
    std::unique_ptr<InstanceBuckets<SkinnedMesh, Instance>> m_instances;
    std::unique_ptr<IBuffer>                                m_paletteBuffer;
    uint32_t                                                m_paletteCapacity = 0; // joint matrices
    std::vector<JointMatrix>                                m_palette; // skinning matrices of the open scope

    bool                     m_inScope = false;
    Mat4                     m_viewProj;
    Vec3                     m_cameraPosition;
    Frustum                  m_frustum;
    MeshLighting             m_lighting;
    SkinnedMeshRendererStats m_stats;
};
