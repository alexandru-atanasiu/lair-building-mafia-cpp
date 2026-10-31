#include "render/mesh_cache.h"

#include "render/mesh.h"

MeshCache::MeshCache(IRenderDevice& device)
    : m_device(device)
{
}

MeshCache::~MeshCache() = default;

std::shared_ptr<const Mesh> MeshCache::Load(const char* path)
{
    auto it = m_meshes.find(path);
    if (it != m_meshes.end())
        return it->second;

    std::shared_ptr<const Mesh> mesh = LoadMesh(m_device, path);
    if (mesh)
        m_meshes.emplace(path, mesh);
    return mesh;
}
