#pragma once

#include "core/math.h"
#include "render/camera_3d.h"

class IWindow;

// Top-down camera orbiting a focus point on a level's floor.
// WASD / arrows pan relative to the view (Shift: faster), the mouse wheel zooms, Q/E turn
// (yaw) and dragging with the right mouse button held turns (left/right) and tilts (up/down).
class OrbitCamera
{
public:
    // `window` must outlive the camera.
    explicit OrbitCamera(IWindow& window);
    ~OrbitCamera();

    OrbitCamera(const OrbitCamera&)            = delete;
    OrbitCamera& operator=(const OrbitCamera&) = delete;

    // Keeps the focus point inside this ground rectangle (x/z).
    void SetBounds(Vec2 min, Vec2 max) { m_boundsMin = min; m_boundsMax = max; }
    // Height of the floor it looks at: the focus point and ScreenToGround stay on it.
    void SetGroundHeight(float y) { m_groundHeight = y; }
    // Looks at `focus` (x/z; y is the ground height) from the default distance and angle.
    void Reset(Vec3 focus);

    // Applies input; ignores keyboard/mouse input the dev UI or game UI is using.
    void Update(float deltaSeconds, bool keyboardAvailable, bool mouseAvailable);

    // The camera owns the right mouse button: a press (while the mouse is available) followed
    // by a drag orbits the camera until the button is released, with the cursor hidden and
    // locked. A press released without dragging is a click, reported here during the pump of
    // the release so the game can act on it (the cursor is still where it was pressed).
    bool IsOrbiting() const { return m_orbiting; }
    bool WasRightClicked() const { return m_rightClicked; }

    // Ray from the camera through a screen position (pixels from the top-left); `direction`
    // isn't normalized.
    void ScreenRay(Vec2 screen, Vec3& origin, Vec3& direction) const;
    // Point of the ground plane under a screen position. False if the ray misses it
    // (looking above the horizon).
    bool ScreenToGround(Vec2 screen, Vec3& ground) const;
    // Screen position of a world point. False if it's behind the camera.
    bool WorldToScreen(Vec3 world, Vec2& screen) const;

    const Camera3D& GetCamera() const { return m_camera; }

#if defined(GAME_IMGUI_ENABLED)
    // Controls help and camera settings, for a DebugPad category.
    void DrawCheats();
#endif

private:
    void UpdateCamera();

    IWindow& m_window;
    Camera3D m_camera;
    Vec3     m_focus;
    float    m_groundHeight = 0.0f;
    float    m_distance = 26.0f;
    float    m_yaw      = 0.0f;
    float    m_pitch    = DegToRad(-55.0f);
    Vec2     m_boundsMin = { -1e6f, -1e6f };
    Vec2     m_boundsMax = { 1e6f, 1e6f };

    // Right mouse button gesture: pressed on the map, then either dragged (orbit) or released
    // in place (click).
    bool    m_rightHeld    = false;
    bool    m_orbiting     = false;
    bool    m_rightClicked = false;
    int32_t m_pressX       = 0;
    int32_t m_pressY       = 0;
};
