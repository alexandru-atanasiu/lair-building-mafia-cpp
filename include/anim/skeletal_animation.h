#pragma once

#include "core/math.h"

#include <cstdint>
#include <memory>

// Skeletal animation runtime, built on ozz-animation (kept out of this header). Assets come
// from cook.bat's skeletal pipeline: <name>.skel, <name>.<action>.anim (see tools/anim_cook).

// Joint hierarchy and rest pose. Joints are ordered parents first.
class Skeleton
{
public:
    struct Impl;

    explicit Skeleton(std::unique_ptr<Impl> impl);
    ~Skeleton();

    Skeleton(const Skeleton&)            = delete;
    Skeleton& operator=(const Skeleton&) = delete;

    uint32_t    GetJointCount() const;
    const char* GetJointName(uint32_t joint) const;
    int         GetJointParent(uint32_t joint) const; // -1 for roots

    const Impl& GetImpl() const { return *m_impl; }

private:
    std::unique_ptr<Impl> m_impl;
};

// One animation (a Blender action): a track per skeleton joint.
class AnimationClip
{
public:
    struct Impl;

    explicit AnimationClip(std::unique_ptr<Impl> impl);
    ~AnimationClip();

    AnimationClip(const AnimationClip&)            = delete;
    AnimationClip& operator=(const AnimationClip&) = delete;

    const char* GetName() const;
    float       GetDuration() const; // seconds
    uint32_t    GetJointCount() const;

    const Impl& GetImpl() const { return *m_impl; }

private:
    std::unique_ptr<Impl> m_impl;
};

// Path relative to the data folder. Log and return null on failure.
std::unique_ptr<Skeleton>      LoadSkeleton(const char* path);
std::unique_ptr<AnimationClip> LoadAnimationClip(const char* path);

struct AnimationLayer
{
    const AnimationClip* clip   = nullptr;
    float                time   = 0.0f; // seconds; wraps around the clip's duration (looping)
    float                weight = 1.0f; // relative to the other layers; <= 0 skips the layer
};

// Per-character animation state: samples and blends clips into model-space joint matrices.
// Clips must have been made for the animator's skeleton.
class Animator
{
public:
    static constexpr uint32_t kMaxLayers = 4;

    // `skeleton` must outlive the animator.
    explicit Animator(const Skeleton& skeleton);
    ~Animator();

    Animator(const Animator&)            = delete;
    Animator& operator=(const Animator&) = delete;

    // Samples each layer, blends them by weight (normalized; the rest pose if no layer
    // contributes) and computes the model-space matrix of every joint.
    void Evaluate(const AnimationLayer* layers, uint32_t layerCount);

    const Skeleton& GetSkeleton() const { return m_skeleton; }
    uint32_t        GetJointCount() const;
    // Joint transforms relative to the skeleton's root space, from the last Evaluate
    // (identity before the first one).
    const Mat4*     GetModelMatrices() const;

private:
    struct Impl;

    const Skeleton&       m_skeleton;
    std::unique_ptr<Impl> m_impl;
};
