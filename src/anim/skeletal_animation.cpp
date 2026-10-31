#include "anim/skeletal_animation.h"

#include "core/assert.h"
#include "core/log.h"

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/blending_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/soa_transform.h"

#include <cmath>
#include <vector>

struct Skeleton::Impl
{
    ozz::animation::Skeleton skeleton;
};

struct AnimationClip::Impl
{
    ozz::animation::Animation animation;
};

namespace
{
    // Reads one ozz object from an archive file, checking its type tag first.
    template <typename T>
    bool ReadArchive(const char* path, T& object)
    {
        ozz::io::File file(path, "rb");
        if (!file.opened())
        {
            LOG_ERR("Can't open '%s'", path);
            return false;
        }
        ozz::io::IArchive archive(&file);
        if (!archive.TestTag<T>())
        {
            LOG_ERR("'%s' doesn't hold the expected ozz object (or its version); re-run cook.bat", path);
            return false;
        }
        archive >> object;
        return true;
    }
}

// ---------------------------------------------------------------------------
// Skeleton / AnimationClip
// ---------------------------------------------------------------------------

Skeleton::Skeleton(std::unique_ptr<Impl> impl)
    : m_impl(std::move(impl))
{
}

Skeleton::~Skeleton() = default;

uint32_t Skeleton::GetJointCount() const
{
    return (uint32_t)m_impl->skeleton.num_joints();
}

const char* Skeleton::GetJointName(uint32_t joint) const
{
    GAME_ASSERT_CRITICAL(joint < GetJointCount(), "Joint %u out of range (%u joints)", joint, GetJointCount());
    return m_impl->skeleton.joint_names()[joint];
}

int Skeleton::GetJointParent(uint32_t joint) const
{
    GAME_ASSERT_CRITICAL(joint < GetJointCount(), "Joint %u out of range (%u joints)", joint, GetJointCount());
    return m_impl->skeleton.joint_parents()[joint];
}

AnimationClip::AnimationClip(std::unique_ptr<Impl> impl)
    : m_impl(std::move(impl))
{
}

AnimationClip::~AnimationClip() = default;

const char* AnimationClip::GetName() const
{
    return m_impl->animation.name();
}

float AnimationClip::GetDuration() const
{
    return m_impl->animation.duration();
}

uint32_t AnimationClip::GetJointCount() const
{
    return (uint32_t)m_impl->animation.num_tracks();
}

std::unique_ptr<Skeleton> LoadSkeleton(const char* path)
{
    auto impl = std::make_unique<Skeleton::Impl>();
    if (!ReadArchive(path, impl->skeleton))
        return nullptr;
    LOG_INFO("Skeleton loaded: %s (%d joints)", path, impl->skeleton.num_joints());
    return std::make_unique<Skeleton>(std::move(impl));
}

std::unique_ptr<AnimationClip> LoadAnimationClip(const char* path)
{
    auto impl = std::make_unique<AnimationClip::Impl>();
    if (!ReadArchive(path, impl->animation))
        return nullptr;
    LOG_INFO("Animation loaded: %s ('%s', %.2f s)", path, impl->animation.name(), impl->animation.duration());
    return std::make_unique<AnimationClip>(std::move(impl));
}

// ---------------------------------------------------------------------------
// Animator
// ---------------------------------------------------------------------------

struct Animator::Impl
{
    // Per layer slot: sampling cache and sampled local transforms (ozz SoA: 4 joints per
    // element). Allocated the first time a slot is used.
    ozz::animation::SamplingJob::Context  contexts[kMaxLayers];
    std::vector<ozz::math::SoaTransform>  layerLocals[kMaxLayers];
    std::vector<ozz::math::SoaTransform>  blendedLocals;
    std::vector<ozz::math::Float4x4>      models;
    std::vector<Mat4>                     modelMatrices;
};

Animator::Animator(const Skeleton& skeleton)
    : m_skeleton(skeleton)
    , m_impl(std::make_unique<Impl>())
{
    const ozz::animation::Skeleton& s = skeleton.GetImpl().skeleton;
    m_impl->blendedLocals.resize(s.num_soa_joints());
    m_impl->models.resize(s.num_joints());
    m_impl->modelMatrices.assign(s.num_joints(), Mat4::Identity());
}

Animator::~Animator() = default;

uint32_t Animator::GetJointCount() const
{
    return (uint32_t)m_impl->modelMatrices.size();
}

const Mat4* Animator::GetModelMatrices() const
{
    return m_impl->modelMatrices.data();
}

void Animator::Evaluate(const AnimationLayer* layers, uint32_t layerCount)
{
    GAME_ASSERT(layerCount <= kMaxLayers, "%u animation layers (max %u)", layerCount, kMaxLayers);
    const ozz::animation::Skeleton& skeleton = m_skeleton.GetImpl().skeleton;

    // Sample every contributing layer.
    ozz::animation::BlendingJob::Layer blendLayers[kMaxLayers];
    uint32_t                           blendCount = 0;
    for (uint32_t i = 0; i < layerCount && i < kMaxLayers; ++i)
    {
        const AnimationLayer& layer = layers[i];
        if (!layer.clip || layer.weight <= 0.0f)
            continue;

        const ozz::animation::Animation& animation = layer.clip->GetImpl().animation;
        if (animation.num_tracks() != skeleton.num_joints())
        {
            GAME_ASSERT(false, "Clip '%s' has %d tracks, skeleton has %d joints",
                        animation.name(), animation.num_tracks(), skeleton.num_joints());
            continue;
        }

        const float duration = animation.duration();
        float       ratio    = duration > 0.0f ? std::fmod(layer.time, duration) / duration : 0.0f;
        if (ratio < 0.0f)
            ratio += 1.0f;

        // Each layer index keeps its own sampling cache, even while other layers are skipped
        // (weight 0): the cache stays warm for the clip that layer plays.
        ozz::animation::SamplingJob::Context& context = m_impl->contexts[i];
        std::vector<ozz::math::SoaTransform>& locals  = m_impl->layerLocals[i];
        if (context.max_tracks() < skeleton.num_joints())
        {
            context.Resize(skeleton.num_joints());
            locals.resize(skeleton.num_soa_joints());
        }

        ozz::animation::SamplingJob sampling;
        sampling.animation = &animation;
        sampling.context   = &context;
        sampling.ratio     = ratio;
        sampling.output    = ozz::make_span(locals);
        if (!sampling.Run())
        {
            GAME_ASSERT(false, "Sampling '%s' failed", animation.name());
            continue;
        }

        blendLayers[blendCount].weight    = layer.weight;
        blendLayers[blendCount].transform = ozz::make_span(locals);
        ++blendCount;
    }

    // Blend (falls back to the rest pose when nothing contributes).
    ozz::animation::BlendingJob blending;
    blending.layers    = ozz::span<const ozz::animation::BlendingJob::Layer>(blendLayers, blendCount);
    blending.rest_pose = skeleton.joint_rest_poses();
    blending.output    = ozz::make_span(m_impl->blendedLocals);
    if (!blending.Run())
    {
        GAME_ASSERT(false, "BlendingJob failed");
        return;
    }

    ozz::animation::LocalToModelJob localToModel;
    localToModel.skeleton = &skeleton;
    localToModel.input    = ozz::make_span(m_impl->blendedLocals);
    localToModel.output   = ozz::make_span(m_impl->models);
    if (!localToModel.Run())
    {
        GAME_ASSERT(false, "LocalToModelJob failed");
        return;
    }

    // ozz matrices are column-major SIMD; ours are row-major with column vectors.
    for (size_t j = 0; j < m_impl->models.size(); ++j)
    {
        Mat4& out = m_impl->modelMatrices[j];
        for (int col = 0; col < 4; ++col)
        {
            float column[4];
            ozz::math::StorePtrU(m_impl->models[j].cols[col], column);
            for (int row = 0; row < 4; ++row)
                out.m[row][col] = column[row];
        }
    }
}
