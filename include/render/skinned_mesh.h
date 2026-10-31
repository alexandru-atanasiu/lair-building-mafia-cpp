#pragma once

#include "core/math.h"

#include <cstdint>
#include <memory>
#include <vector>

class IBuffer;
class IRenderDevice;

// Cooked skinned mesh file (.skin), written by tools/anim_cook. Little endian:
//   SkinnedMeshFileHeader, then vertexCount SkinnedVertex, then indexCount uint32 indices
//   (triangle list, clockwise front faces), then jointCount inverse bind matrices
//   (float[3][4], rows of the model-space bind pose inverse) indexed like the skeleton's joints.
struct SkinnedMeshFileHeader
{
    char     magic[4]; // "DXSM"
    uint32_t version;  // kSkinnedMeshFileVersion
    uint32_t vertexCount;
    uint32_t indexCount;
    uint32_t jointCount; // must match the skeleton the mesh is drawn with
    float    sphereCenter[3]; // bind pose bounding sphere, model space
    float    sphereRadius;
};
static_assert(sizeof(SkinnedMeshFileHeader) == 36);

constexpr uint32_t kSkinnedMeshFileVersion = 2;
constexpr uint32_t kMaxSkinJoints          = 256; // joint indices are 8-bit

struct SkinnedVertex
{
    float   position[3];
    float   normal[3];
    uint8_t joints[4];  // skeleton joint indices
    float   weights[4]; // sum to 1
    uint8_t color[4];   // RGBA8 material color, multiplied by the instance color
};
static_assert(sizeof(SkinnedVertex) == 48);

// Immutable GPU skinned mesh: vertex + index buffers, inverse bind matrices and bounds.
class SkinnedMesh
{
public:
    SkinnedMesh(std::unique_ptr<IBuffer> vertices, std::unique_ptr<IBuffer> indices, uint32_t indexCount,
                std::vector<Mat4> inverseBinds, Vec3 boundsCenter, float boundsRadius);
    ~SkinnedMesh();

    SkinnedMesh(const SkinnedMesh&)            = delete;
    SkinnedMesh& operator=(const SkinnedMesh&) = delete;

    const IBuffer& GetVertexBuffer() const { return *m_vertices; }
    const IBuffer& GetIndexBuffer() const { return *m_indices; }
    uint32_t       GetIndexCount() const { return m_indexCount; }

    uint32_t    GetJointCount() const { return (uint32_t)m_inverseBinds.size(); }
    const Mat4* GetInverseBinds() const { return m_inverseBinds.data(); }

    // Bounding sphere of the bind pose, model space.
    Vec3  GetBoundsCenter() const { return m_boundsCenter; }
    float GetBoundsRadius() const { return m_boundsRadius; }

private:
    std::unique_ptr<IBuffer> m_vertices;
    std::unique_ptr<IBuffer> m_indices;
    uint32_t                 m_indexCount;
    std::vector<Mat4>        m_inverseBinds;
    Vec3                     m_boundsCenter;
    float                    m_boundsRadius;
};

// Loads a cooked .skin file (path relative to the data folder). Logs and returns null on failure.
std::unique_ptr<SkinnedMesh> LoadSkinnedMesh(IRenderDevice& device, const char* path);
