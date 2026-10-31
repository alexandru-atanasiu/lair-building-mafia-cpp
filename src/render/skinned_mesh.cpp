#include "render/skinned_mesh.h"

#include "core/file.h"
#include "core/log.h"
#include "render/render_device.h"

#include <cstring>

SkinnedMesh::SkinnedMesh(std::unique_ptr<IBuffer> vertices, std::unique_ptr<IBuffer> indices, uint32_t indexCount,
                         std::vector<Mat4> inverseBinds, Vec3 boundsCenter, float boundsRadius)
    : m_vertices(std::move(vertices))
    , m_indices(std::move(indices))
    , m_indexCount(indexCount)
    , m_inverseBinds(std::move(inverseBinds))
    , m_boundsCenter(boundsCenter)
    , m_boundsRadius(boundsRadius)
{
}

SkinnedMesh::~SkinnedMesh() = default;

std::unique_ptr<SkinnedMesh> LoadSkinnedMesh(IRenderDevice& device, const char* path)
{
    std::vector<uint8_t> bytes;
    if (!ReadFileBytes(path, bytes))
    {
        LOG_ERR("Can't read skinned mesh '%s'", path);
        return nullptr;
    }

    SkinnedMeshFileHeader header;
    if (bytes.size() < sizeof(header))
    {
        LOG_ERR("Skinned mesh '%s' is truncated (%zu bytes)", path, bytes.size());
        return nullptr;
    }
    memcpy(&header, bytes.data(), sizeof(header));

    if (memcmp(header.magic, "DXSM", 4) != 0 || header.version != kSkinnedMeshFileVersion)
    {
        LOG_ERR("Skinned mesh '%s' isn't a version %u .skin file; re-run cook.bat", path, kSkinnedMeshFileVersion);
        return nullptr;
    }
    if (header.vertexCount == 0 || header.indexCount == 0 || header.indexCount % 3 != 0 ||
        header.jointCount == 0 || header.jointCount > kMaxSkinJoints)
    {
        LOG_ERR("Skinned mesh '%s' has %u vertices / %u indices / %u joints", path,
                header.vertexCount, header.indexCount, header.jointCount);
        return nullptr;
    }

    const size_t verticesSize = (size_t)header.vertexCount * sizeof(SkinnedVertex);
    const size_t indicesSize  = (size_t)header.indexCount * sizeof(uint32_t);
    const size_t bindsSize    = (size_t)header.jointCount * 12 * sizeof(float);
    if (bytes.size() != sizeof(header) + verticesSize + indicesSize + bindsSize)
    {
        LOG_ERR("Skinned mesh '%s' is %zu bytes, expected %zu", path, bytes.size(),
                sizeof(header) + verticesSize + indicesSize + bindsSize);
        return nullptr;
    }

    // Validate what the GPU will index with before it gets there.
    const uint8_t* verticesData = bytes.data() + sizeof(header);
    for (uint32_t v = 0; v < header.vertexCount; ++v)
    {
        SkinnedVertex vertex;
        memcpy(&vertex, verticesData + (size_t)v * sizeof(SkinnedVertex), sizeof(vertex));
        for (uint8_t joint : vertex.joints)
        {
            if (joint >= header.jointCount)
            {
                LOG_ERR("Skinned mesh '%s': vertex %u uses joint %u (%u joints)", path, v, joint, header.jointCount);
                return nullptr;
            }
        }
    }
    std::vector<uint32_t> indices(header.indexCount);
    memcpy(indices.data(), verticesData + verticesSize, indicesSize);
    for (uint32_t index : indices)
    {
        if (index >= header.vertexCount)
        {
            LOG_ERR("Skinned mesh '%s' has an out of range index %u (%u vertices)", path, index, header.vertexCount);
            return nullptr;
        }
    }

    std::vector<Mat4> inverseBinds(header.jointCount, Mat4::Identity());
    const uint8_t*    bindsData = verticesData + verticesSize + indicesSize;
    for (uint32_t j = 0; j < header.jointCount; ++j)
        memcpy(inverseBinds[j].m, bindsData + (size_t)j * 12 * sizeof(float), 12 * sizeof(float)); // rows 0..2

    auto vb = device.CreateVertexBuffer(verticesData, (uint32_t)verticesSize);
    auto ib = device.CreateIndexBuffer(indices.data(), header.indexCount);

    LOG_INFO("Skinned mesh loaded: %s (%u vertices, %u triangles, %u joints)", path,
             header.vertexCount, header.indexCount / 3, header.jointCount);
    return std::make_unique<SkinnedMesh>(std::move(vb), std::move(ib), header.indexCount, std::move(inverseBinds),
                                         Vec3{ header.sphereCenter[0], header.sphereCenter[1], header.sphereCenter[2] },
                                         header.sphereRadius);
}
