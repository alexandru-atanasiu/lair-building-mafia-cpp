#pragma once

#include "core/math.h"

#include <cstdint>

// Orthographic 2D camera. World space is y-up; at zoom 1 one world unit covers one pixel.
// Screen space is in pixels from the top-left of the viewport, y-down.
class Camera2D
{
public:
    static constexpr float kMinZoom = 1.0f / 64.0f;
    static constexpr float kMaxZoom = 64.0f;

    // Call when the render target size changes (e.g. every frame with the window size).
    void SetViewportSize(uint32_t width, uint32_t height);
    Vec2 GetViewportSize() const { return m_viewport; }

    // World point shown at the center of the viewport.
    Vec2 GetPosition() const { return m_position; }
    void SetPosition(Vec2 position) { m_position = position; }
    void Move(Vec2 delta) { m_position += delta; }

    // Pixels per world unit, clamped to [kMinZoom, kMaxZoom].
    float GetZoom() const { return m_zoom; }
    void  SetZoom(float zoom);
    // Multiplies the zoom, keeping the world point under `screenPoint` where it is.
    void  ZoomAt(Vec2 screenPoint, float factor);

    Vec2 ScreenToWorld(Vec2 screen) const;
    Vec2 WorldToScreen(Vec2 world) const;

    // World-space rectangle covered by the viewport.
    void GetVisibleBounds(Vec2& min, Vec2& max) const;

    // clip = world * scale + offset (per axis).
    void GetWorldToClip(Vec2& scale, Vec2& offset) const;

private:
    Vec2  m_position;
    Vec2  m_viewport = { 1.0f, 1.0f };
    float m_zoom     = 1.0f;
};
