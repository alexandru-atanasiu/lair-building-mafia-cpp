#pragma once

#include "anim/skeletal_animation.h"
#include "character_motion.h"
#include "core/color.h"
#include "render/skinned_mesh_renderer.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

class IRenderDevice;
class SkinnedMesh;

// Draws everyone who walks: the soldier rig (assets/animated/soldier.blend) cross-fading between
// its idle and run clips by each CharacterMotion. Ids are the caller's and must be unique among
// all characters drawn through this view.
class CharacterView
{
public:
    explicit CharacterView(IRenderDevice& device);
    ~CharacterView();

    // Advances the character's animation; call once per frame while it's visible.
    void Animate(uint32_t id, const CharacterMotion& motion, float simSeconds);

    // Draws with the pose of the last Animate. The body's material is near white, so `tint` is
    // its color; the joints stay near black.
    void Draw(SkinnedMeshRenderer::Scope& scope, uint32_t id, const CharacterMotion& motion, const Color& tint) const;

    // The character is gone for good: frees its animation state.
    void Forget(uint32_t id);

private:
    enum Clip
    {
        Idle,
        Run,
        ClipCount,
    };

    struct Pose
    {
        explicit Pose(const Skeleton& skeleton) : animator(skeleton) {}

        Animator animator;
        float    time[ClipCount]   = {};
        float    weight[ClipCount] = { 1.0f, 0.0f };
    };

    std::unique_ptr<Skeleton>      m_skeleton;
    std::unique_ptr<SkinnedMesh>   m_mesh;
    std::unique_ptr<AnimationClip> m_clips[ClipCount];

    // Poses are heavy and hold references: stable addresses.
    std::unordered_map<uint32_t, std::unique_ptr<Pose>> m_poses;
};
