#include "render/mesh.h"

#include "core/file.h"
#include "core/log.h"
#include "render/render_device.h"

#include <cstring>
#include <vector>

Mesh::Mesh(std::unique_ptr<IBuffer> vertices, std::unique_ptr<IBuffer> indices, uint32_t indexCount,
           Vec3 boundsCenter, float boundsRadius)
    : m_vertices(std::move(vertices))
    , m_indices(std::move(indices))
    , m_indexCount(indexCount)
    , m_boundsCenter(boundsCenter)
    , m_boundsRadius(boundsRadius)
{
}

Mesh::~Mesh() = default;

std::unique_ptr<Mesh> LoadMesh(IRenderDevice& device, const char* path)
{
    std::vector<uint8_t> bytes;
    if (!ReadFileBytes(path, bytes))
    {
        LOG_ERR("Can't read mesh '%s'", path);
        return nullptr;
    }

    MeshFileHeader header;
    if (bytes.size() < sizeof(header))
    {
        LOG_ERR("Mesh '%s' is truncated (%zu bytes)", path, bytes.size());
        return nullptr;
    }
    memcpy(&header, bytes.data(), sizeof(header));

    if (memcmp(header.magic, "DXMS", 4) != 0 || header.version != kMeshFileVersion)
    {
        LOG_ERR("Mesh '%s' isn't a version %u .mesh file; re-run cook.bat", path, kMeshFileVersion);
        return nullptr;
    }
    if (header.vertexCount == 0 || header.indexCount == 0 || header.indexCount % 3 != 0)
    {
        LOG_ERR("Mesh '%s' has %u vertices / %u indices", path, header.vertexCount, header.indexCount);
        return nullptr;
    }

    const size_t verticesSize = (size_t)header.vertexCount * sizeof(MeshVertex);
    const size_t indicesSize  = (size_t)header.indexCount * sizeof(uint32_t);
    if (bytes.size() != sizeof(header) + verticesSize + indicesSize)
    {
        LOG_ERR("Mesh '%s' is %zu bytes, expected %zu", path, bytes.size(), sizeof(header) + verticesSize + indicesSize);
        return nullptr;
    }

    // Indices are validated before they reach the GPU (copied out to avoid aliasing the bytes).
    std::vector<uint32_t> indices(header.indexCount);
    memcpy(indices.data(), bytes.data() + sizeof(header) + verticesSize, indicesSize);
    for (uint32_t index : indices)
    {
        if (index >= header.vertexCount)
        {
            LOG_ERR("Mesh '%s' has an out of range index %u (%u vertices)", path, index, header.vertexCount);
            return nullptr;
        }
    }

    auto vb = device.CreateVertexBuffer(bytes.data() + sizeof(header), (uint32_t)verticesSize);
    auto ib = device.CreateIndexBuffer(indices.data(), header.indexCount);

    LOG_INFO("Mesh loaded: %s (%u vertices, %u triangles)", path, header.vertexCount, header.indexCount / 3);
    return std::make_unique<Mesh>(std::move(vb), std::move(ib), header.indexCount,
                                  Vec3{ header.sphereCenter[0], header.sphereCenter[1], header.sphereCenter[2] },
                                  header.sphereRadius);
}
