#include "character_view.h"

#include "core/fatal.h"
#include "render/skinned_mesh.h"

#include <algorithm>
#include <cmath>

namespace
{
    // Cooked from assets/animated/soldier.blend (py/create_soldier.py). It also has a "die" clip.
    constexpr const char* kSkeletonPath = "animated/soldier.skel";
    constexpr const char* kMeshPath     = "animated/soldier.skin";
    constexpr const char* kIdleClipPath = "animated/soldier.idle.anim";
    constexpr const char* kRunClipPath  = "animated/soldier.run.anim";

    constexpr float kBlendSeconds    = 0.15f; // animation cross-fade
    constexpr float kRunCycleMetres  = 3.2f;  // distance covered by one loop of the run clip
}

CharacterView::CharacterView(IRenderDevice& device)
    : m_skeleton(LoadOrFatal(LoadSkeleton(kSkeletonPath), kSkeletonPath))
    , m_mesh(LoadOrFatal(LoadSkinnedMesh(device, kMeshPath), kMeshPath))
{
    m_clips[Idle] = LoadOrFatal(LoadAnimationClip(kIdleClipPath), kIdleClipPath);
    m_clips[Run]  = LoadOrFatal(LoadAnimationClip(kRunClipPath), kRunClipPath);
    if (m_mesh->GetJointCount() != m_skeleton->GetJointCount())
        Fatal("Character mesh and skeleton joint counts differ");
}

CharacterView::~CharacterView() = default;

void CharacterView::Animate(uint32_t id, const CharacterMotion& motion, float simSeconds)
{
    std::unique_ptr<Pose>& slot = m_poses[id];
    if (!slot)
    {
        slot = std::make_unique<Pose>(*m_skeleton);
        // Characters don't walk in step.
        slot->time[Idle] = std::fmod((float)id * 0.37f, m_clips[Idle]->GetDuration());
    }
    Pose& pose = *slot;

    const Clip  wanted = motion.IsMoving() ? Run : Idle;
    const float step   = simSeconds / kBlendSeconds;
    for (int c = 0; c < ClipCount; ++c)
    {
        pose.weight[c] = c == wanted ? std::min(pose.weight[c] + step, 1.0f) : std::max(pose.weight[c] - step, 0.0f);

        // The run clip's rate matches the walking speed, so feet don't slide.
        const float duration = m_clips[c]->GetDuration();
        const float rate     = c == Run ? motion.speed / kRunCycleMetres * duration : 1.0f;
        if (pose.weight[c] > 0.0f)
            pose.time[c] = std::fmod(pose.time[c] + simSeconds * rate, duration);
    }

    const AnimationLayer layers[ClipCount] = {
        { m_clips[Idle].get(), pose.time[Idle], pose.weight[Idle] },
        { m_clips[Run].get(), pose.time[Run], pose.weight[Run] },
    };
    pose.animator.Evaluate(layers, ClipCount);
}

void CharacterView::Draw(SkinnedMeshRenderer::Scope& scope, uint32_t id, const CharacterMotion& motion, const Color& tint) const
{
    const auto found = m_poses.find(id);
    if (found == m_poses.end())
        return;
    const Mat4 world = Mat4::Translation(motion.position) * Mat4::RotationY(motion.yaw);
    scope.Draw(*m_mesh, world, found->second->animator.GetModelMatrices(), tint);
}

void CharacterView::Forget(uint32_t id)
{
    m_poses.erase(id);
}
