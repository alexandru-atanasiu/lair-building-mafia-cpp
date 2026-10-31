#pragma once

#include "core/math.h"

#include <cstdint>

// View frustum as 6 inward-facing planes (a, b, c, d): inside where a*x + b*y + c*z + d >= 0.
struct Frustum
{
    float planes[6][4] = {};

    // Extracted from a view-projection matrix (D3D clip space, depth in [0, 1]).
    static Frustum FromViewProjection(const Mat4& viewProj);

    // False if the sphere is entirely outside (conservative: may keep some outside spheres).
    bool IntersectsSphere(Vec3 center, float radius) const;
};

// Perspective camera oriented by yaw and pitch (no roll), in the engine's left-handed Y-up
// world. Yaw 0 looks down +Z and positive yaw turns right (toward +X); positive pitch looks up.
class Camera3D
{
public:
    static constexpr float kMaxPitch = 1.55f; // just under 90 degrees: never looks straight up/down

    // Call when the render target size changes (e.g. every frame with the window size).
    void SetViewportSize(uint32_t width, uint32_t height);
    float GetAspect() const { return m_aspect; }

    Vec3 GetPosition() const { return m_position; }
    void SetPosition(Vec3 position) { m_position = position; }
    void Move(Vec3 delta) { m_position += delta; }

    float GetYaw() const { return m_yaw; }
    float GetPitch() const { return m_pitch; }
    // Pitch is clamped to [-kMaxPitch, kMaxPitch]; yaw is wrapped to [-pi, pi].
    void  SetOrientation(float yaw, float pitch);
    void  Rotate(float deltaYaw, float deltaPitch) { SetOrientation(m_yaw + deltaYaw, m_pitch + deltaPitch); }
    void  LookAt(Vec3 target);

    float GetVerticalFov() const { return m_verticalFov; }
    void  SetVerticalFov(float radians) { m_verticalFov = radians; }
    float GetNearZ() const { return m_nearZ; }
    float GetFarZ() const { return m_farZ; }
    void  SetDepthRange(float nearZ, float farZ);

    // Orthonormal basis.
    Vec3 GetForward() const;
    Vec3 GetRight() const;
    Vec3 GetUp() const;

    Mat4 GetView() const;
    Mat4 GetProjection() const;
    Mat4 GetViewProjection() const { return GetProjection() * GetView(); }

private:
    Vec3  m_position;
    float m_yaw         = 0.0f;
    float m_pitch       = 0.0f;
    float m_verticalFov = 1.0471976f; // 60 degrees
    float m_nearZ       = 0.1f;
    float m_farZ        = 1000.0f;
    float m_aspect      = 16.0f / 9.0f;
};
