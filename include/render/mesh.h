#pragma once

#include "core/math.h"

#include <cstdint>
#include <memory>

class IBuffer;
class IRenderDevice;

// Cooked mesh file (.mesh), written by py/pack_mesh.py (Blender). Little endian:
//   MeshFileHeader, then vertexCount MeshVertex, then indexCount uint32 indices
//   (triangle list, clockwise front faces, left-handed Y-up space).
struct MeshFileHeader
{
    char     magic[4]; // "DXMS"
    uint32_t version;  // kMeshFileVersion
    uint32_t vertexCount;
    uint32_t indexCount;
    float    boundsMin[3];
    float    boundsMax[3];
    float    sphereCenter[3];
    float    sphereRadius;
};
static_assert(sizeof(MeshFileHeader) == 56);

constexpr uint32_t kMeshFileVersion = 1;

struct MeshVertex
{
    float position[3];
    float normal[3];
};
static_assert(sizeof(MeshVertex) == 24);

// Immutable GPU mesh: vertex + index buffers and object-space bounds.
class Mesh
{
public:
    Mesh(std::unique_ptr<IBuffer> vertices, std::unique_ptr<IBuffer> indices, uint32_t indexCount,
         Vec3 boundsCenter, float boundsRadius);
    ~Mesh();

    Mesh(const Mesh&)            = delete;
    Mesh& operator=(const Mesh&) = delete;

    const IBuffer& GetVertexBuffer() const { return *m_vertices; }
    const IBuffer& GetIndexBuffer() const { return *m_indices; }
    uint32_t       GetIndexCount() const { return m_indexCount; }

    // Bounding sphere in object space.
    Vec3  GetBoundsCenter() const { return m_boundsCenter; }
    float GetBoundsRadius() const { return m_boundsRadius; }

private:
    std::unique_ptr<IBuffer> m_vertices;
    std::unique_ptr<IBuffer> m_indices;
    uint32_t                 m_indexCount;
    Vec3                     m_boundsCenter;
    float                    m_boundsRadius;
};

// Loads a cooked .mesh file (path relative to the data folder). Logs and returns null on failure.
std::unique_ptr<Mesh> LoadMesh(IRenderDevice& device, const char* path);
