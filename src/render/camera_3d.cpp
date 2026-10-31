#include "render/camera_3d.h"

#include <algorithm>
#include <cmath>

Frustum Frustum::FromViewProjection(const Mat4& viewProj)
{
    // Gribb/Hartmann: with clip = M * p, a point is inside when -w <= x <= w, -w <= y <= w
    // and 0 <= z <= w, which gives each plane as a combination of M's rows.
    const auto& m = viewProj.m;
    Frustum f;
    for (int col = 0; col < 4; ++col)
    {
        f.planes[0][col] = m[3][col] + m[0][col]; // left
        f.planes[1][col] = m[3][col] - m[0][col]; // right
        f.planes[2][col] = m[3][col] + m[1][col]; // bottom
        f.planes[3][col] = m[3][col] - m[1][col]; // top
        f.planes[4][col] = m[2][col];             // near
        f.planes[5][col] = m[3][col] - m[2][col]; // far
    }
    for (auto& p : f.planes)
    {
        const float len = std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
        if (len > 0.0f)
            for (float& v : p)
                v /= len;
    }
    return f;
}

bool Frustum::IntersectsSphere(Vec3 center, float radius) const
{
    for (const auto& p : planes)
    {
        if (p[0] * center.x + p[1] * center.y + p[2] * center.z + p[3] < -radius)
            return false;
    }
    return true;
}

void Camera3D::SetViewportSize(uint32_t width, uint32_t height)
{
    m_aspect = (float)std::max(width, 1u) / (float)std::max(height, 1u);
}

void Camera3D::SetOrientation(float yaw, float pitch)
{
    m_yaw   = std::remainder(yaw, 2.0f * kPi);
    m_pitch = std::clamp(pitch, -kMaxPitch, kMaxPitch);
}

void Camera3D::LookAt(Vec3 target)
{
    const Vec3  dir        = target - m_position;
    const float horizontal = std::sqrt(dir.x * dir.x + dir.z * dir.z);
    if (horizontal > 0.0f || dir.y != 0.0f)
        SetOrientation(std::atan2(dir.x, dir.z), std::atan2(dir.y, horizontal));
}

void Camera3D::SetDepthRange(float nearZ, float farZ)
{
    m_nearZ = std::max(nearZ, 1e-4f);
    m_farZ  = std::max(farZ, m_nearZ * 2.0f);
}

Vec3 Camera3D::GetForward() const
{
    const float cp = std::cos(m_pitch);
    return { std::sin(m_yaw) * cp, std::sin(m_pitch), std::cos(m_yaw) * cp };
}

Vec3 Camera3D::GetRight() const
{
    return { std::cos(m_yaw), 0.0f, -std::sin(m_yaw) };
}

Vec3 Camera3D::GetUp() const
{
    return Cross(GetForward(), GetRight());
}

Mat4 Camera3D::GetView() const
{
    return Mat4::View(m_position, GetRight(), GetUp(), GetForward());
}

Mat4 Camera3D::GetProjection() const
{
    return Mat4::PerspectiveFov(m_verticalFov, m_aspect, m_nearZ, m_farZ);
}
