#pragma once

#include <memory>
#include <string>
#include <unordered_map>

class IRenderDevice;
class Mesh;

// Shares meshes between their users: each path is loaded (and uploaded to the GPU) once.
// Meshes stay cached for the cache's lifetime; holders of a returned pointer may keep the mesh
// longer. The device must outlive the cache and every mesh it returned.
class MeshCache
{
public:
    explicit MeshCache(IRenderDevice& device);
    ~MeshCache();

    MeshCache(const MeshCache&)            = delete;
    MeshCache& operator=(const MeshCache&) = delete;

    // LoadMesh on the first request for `path` (relative to the data folder), the same mesh
    // afterwards. Null if loading failed (logged; failures aren't cached, the next call retries).
    std::shared_ptr<const Mesh> Load(const char* path);

private:
    IRenderDevice&                                               m_device;
    std::unordered_map<std::string, std::shared_ptr<const Mesh>> m_meshes;
};
