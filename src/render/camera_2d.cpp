#include "render/camera_2d.h"

#include <algorithm>

void Camera2D::SetViewportSize(uint32_t width, uint32_t height)
{
    m_viewport = { (float)std::max(width, 1u), (float)std::max(height, 1u) };
}

void Camera2D::SetZoom(float zoom)
{
    m_zoom = std::clamp(zoom, kMinZoom, kMaxZoom);
}

void Camera2D::ZoomAt(Vec2 screenPoint, float factor)
{
    const Vec2 before = ScreenToWorld(screenPoint);
    SetZoom(m_zoom * factor);
    m_position += before - ScreenToWorld(screenPoint);
}

Vec2 Camera2D::ScreenToWorld(Vec2 screen) const
{
    const Vec2 fromCenter = screen - m_viewport * 0.5f;
    return { m_position.x + fromCenter.x / m_zoom, m_position.y - fromCenter.y / m_zoom };
}

Vec2 Camera2D::WorldToScreen(Vec2 world) const
{
    const Vec2 fromCenter = (world - m_position) * m_zoom;
    return { m_viewport.x * 0.5f + fromCenter.x, m_viewport.y * 0.5f - fromCenter.y };
}

void Camera2D::GetVisibleBounds(Vec2& min, Vec2& max) const
{
    const Vec2 halfExtent = m_viewport * (0.5f / m_zoom);
    min = m_position - halfExtent;
    max = m_position + halfExtent;
}

void Camera2D::GetWorldToClip(Vec2& scale, Vec2& offset) const
{
    // Clip space spans [-1, 1] over the viewport, y-up like the world.
    scale  = { 2.0f * m_zoom / m_viewport.x, 2.0f * m_zoom / m_viewport.y };
    offset = { -m_position.x * scale.x, -m_position.y * scale.y };
}
