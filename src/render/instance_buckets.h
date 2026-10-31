#pragma once

#include "core/assert.h"
#include "render/render_device.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <memory>
#include <vector>

struct InstanceBucketsFlushStats
{
    uint32_t instances = 0; // drawn
    uint32_t uploads   = 0; // instance buffer refills
    uint32_t drawCalls = 0;
};

// Instances of an opaque, depth-tested renderer, grouped by mesh so each mesh is one draw call
// whatever the submission order. Owns the instance buffer (stream 1). Shared by MeshRenderer
// and SkinnedMeshRenderer; MeshT needs GetVertexBuffer(), GetIndexBuffer(), GetIndexCount().
//
// Meshes are only referenced until the next Flush (they may be destroyed after it).
template <typename MeshT, typename InstanceT>
class InstanceBuckets
{
public:
    // The instance buffer grows to fit a flush up to `maxCapacity` instances; bigger flushes
    // are uploaded and drawn in several chunks.
    InstanceBuckets(IRenderDevice& device, uint32_t minCapacity, uint32_t maxCapacity)
        : m_device(device)
        , m_minCapacity(minCapacity)
        , m_maxCapacity(maxCapacity)
    {
    }

    // Returns a new instance for `mesh`, to be filled in by the caller.
    InstanceT& Add(const MeshT& mesh)
    {
        // Consecutive adds often share a mesh: check the last bucket first.
        if (m_lastBucket >= m_buckets.size() || m_buckets[m_lastBucket].mesh != &mesh)
        {
            auto it = std::find_if(m_buckets.begin(), m_buckets.end(), [&](const Bucket& b) { return b.mesh == &mesh; });
            if (it == m_buckets.end())
            {
                m_buckets.emplace_back().mesh = &mesh;
                it = m_buckets.end() - 1;
            }
            m_lastBucket = (size_t)(it - m_buckets.begin());
        }
        ++m_count;
        return m_buckets[m_lastBucket].instances.emplace_back();
    }

    // Instances added since the last Flush.
    uint32_t GetCount() const { return m_count; }

    // Uploads every instance and draws each mesh's instances with `draw` (pipeline, constants
    // etc. set by the caller; mesh buffers, instance buffer and ranges set here). Empties
    // the buckets.
    InstanceBucketsFlushStats Flush(DrawDesc draw)
    {
        // Concatenate the buckets: each mesh's instances become one contiguous range.
        m_ranges.clear();
        m_upload.clear();
        for (Bucket& bucket : m_buckets)
        {
            if (bucket.instances.empty())
            {
                // Not drawn since the last flush: forget the mesh, which may be destroyed.
                bucket.mesh = nullptr;
                continue;
            }
            m_ranges.push_back({ bucket.mesh, (uint32_t)m_upload.size(), (uint32_t)bucket.instances.size() });
            m_upload.insert(m_upload.end(), bucket.instances.begin(), bucket.instances.end());
            bucket.instances.clear(); // keeps the capacity for the next flush
        }
        std::erase_if(m_buckets, [](const Bucket& b) { return b.mesh == nullptr; });
        m_lastBucket = 0;
        m_count      = 0;

        InstanceBucketsFlushStats stats;
        const uint32_t count = (uint32_t)m_upload.size();
        if (count == 0)
            return stats;
        stats.instances = count;

        EnsureCapacity(std::min(count, m_maxCapacity));
        draw.instanceBuffer = m_instanceBuffer.get();

        for (uint32_t chunkStart = 0; chunkStart < count; chunkStart += m_capacity)
        {
            const uint32_t chunkEnd = std::min(chunkStart + m_capacity, count);
            m_device.UpdateBuffer(*m_instanceBuffer, &m_upload[chunkStart], (chunkEnd - chunkStart) * (uint32_t)sizeof(InstanceT));
            ++stats.uploads;

            // One draw per mesh range overlapping this chunk.
            for (const Range& range : m_ranges)
            {
                const uint32_t first = std::max(range.start, chunkStart);
                const uint32_t last  = std::min(range.start + range.count, chunkEnd);
                if (first >= last)
                    continue;

                draw.vertexBuffer  = &range.mesh->GetVertexBuffer();
                draw.indexBuffer   = &range.mesh->GetIndexBuffer();
                draw.indexCount    = range.mesh->GetIndexCount();
                draw.firstInstance = first - chunkStart;
                draw.instanceCount = last - first;
                m_device.Draw(draw);
                ++stats.drawCalls;
            }
        }
        return stats;
    }

private:
    struct Bucket
    {
        const MeshT*           mesh = nullptr;
        std::vector<InstanceT> instances;
    };

    struct Range
    {
        const MeshT* mesh;
        uint32_t     start;
        uint32_t     count;
    };

    void EnsureCapacity(uint32_t instanceCount)
    {
        if (instanceCount <= m_capacity)
            return;
        m_capacity       = std::clamp(std::bit_ceil(instanceCount), m_minCapacity, m_maxCapacity);
        m_instanceBuffer = m_device.CreateDynamicVertexBuffer(m_capacity * (uint32_t)sizeof(InstanceT));
    }

    IRenderDevice&           m_device;
    uint32_t                 m_minCapacity;
    uint32_t                 m_maxCapacity;
    std::unique_ptr<IBuffer> m_instanceBuffer;
    uint32_t                 m_capacity = 0; // instances m_instanceBuffer holds

    // Kept across flushes so the vectors keep their capacity.
    std::vector<Bucket>    m_buckets;
    size_t                 m_lastBucket = 0;
    uint32_t               m_count      = 0;
    std::vector<InstanceT> m_upload; // all buckets back to back
    std::vector<Range>     m_ranges;
};
