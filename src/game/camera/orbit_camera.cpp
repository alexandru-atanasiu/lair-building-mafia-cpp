#include "orbit_camera.h"

#include "platform/window.h"

#if defined(GAME_IMGUI_ENABLED)
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kPanSpeed        = 1.2f;  // focus distances per second, so it feels the same at any zoom
    constexpr float kFastPanScale    = 2.5f;  // with Shift held
    constexpr float kZoomPerNotch    = 1.15f; // distance factor per mouse wheel notch
    constexpr float kMinDistance     = 8.0f;
    constexpr float kMaxDistance     = 90.0f;
    constexpr float kMinPitch        = DegToRad(-89.0f); // steepest (almost straight down)
    constexpr float kMaxPitch        = DegToRad(-20.0f); // flattest
    constexpr float kKeyTurnSpeed    = DegToRad(90.0f);  // Q/E yaw, radians per second
    constexpr float kTurnPerCount    = 0.004f;           // right-drag yaw, radians per raw mouse count
    constexpr float kTiltPerCount    = 0.003f;           // right-drag pitch, radians per raw mouse count
    constexpr float kDragThresholdPx = 4.0f;             // at 96 DPI: a right press moved further is a drag, not a click
}

OrbitCamera::OrbitCamera(IWindow& window)
    : m_window(window)
{
    m_camera.SetDepthRange(0.5f, 500.0f);
    UpdateCamera();
}

OrbitCamera::~OrbitCamera()
{
    if (m_orbiting)
        m_window.SetMouseCaptured(false);
}

void OrbitCamera::Reset(Vec3 focus)
{
    m_focus    = focus;
    m_distance = m_defaultDistance;
    m_yaw      = m_defaultYaw;
    m_pitch    = m_defaultPitch;
    UpdateCamera();
}

void OrbitCamera::Update(float deltaSeconds, bool keyboardAvailable, bool mouseAvailable)
{
    m_camera.SetViewportSize(m_window.GetWidth(), m_window.GetHeight());

    // Right button: a press the UI doesn't take starts a gesture that becomes an orbit once the
    // cursor moves far enough, and a click otherwise. Once started it ignores `mouseAvailable`
    // (the hidden cursor may sit over a dev UI window) and ends when the button is up, which
    // also covers losing focus (the window releases buttons and the capture then).
    m_rightClicked = false;
    if (m_window.WasMouseButtonPressed(MouseButton::Right) && mouseAvailable && !m_rightHeld)
    {
        m_rightHeld = true;
        m_orbiting  = false;
        m_pressX    = m_window.GetMouseX();
        m_pressY    = m_window.GetMouseY();
    }
    if (m_rightHeld)
    {
        if (!m_orbiting)
        {
            const float threshold = kDragThresholdPx * m_window.GetDpiScale();
            const float dx        = (float)(m_window.GetMouseX() - m_pressX);
            const float dy        = (float)(m_window.GetMouseY() - m_pressY);
            if (dx * dx + dy * dy > threshold * threshold)
            {
                m_orbiting = true;
                m_window.SetMouseCaptured(true);
            }
        }
        if (m_orbiting)
        {
            // Mouse right turns right; mouse up looks down more steeply (the camera rises).
            m_yaw += m_window.GetMouseDeltaX() * kTurnPerCount;
            m_pitch = std::clamp(m_pitch + m_window.GetMouseDeltaY() * kTiltPerCount, kMinPitch, kMaxPitch);
        }
        if (!m_window.IsMouseButtonDown(MouseButton::Right))
        {
            m_rightClicked = !m_orbiting && m_window.WasMouseButtonReleased(MouseButton::Right);
            if (m_orbiting)
                m_window.SetMouseCaptured(false);
            m_rightHeld = false;
            m_orbiting  = false;
        }
    }

    // Middle button: the ground under the cursor follows it.
    if (m_window.WasMouseButtonPressed(MouseButton::Middle) && mouseAvailable)
    {
        m_panning = true;
        m_panX    = m_window.GetMouseX();
        m_panY    = m_window.GetMouseY();
    }
    if (m_panning)
    {
        Vec3 before, after;
        if (ScreenToGround({ (float)m_panX, (float)m_panY }, before) &&
            ScreenToGround({ (float)m_window.GetMouseX(), (float)m_window.GetMouseY() }, after))
        {
            m_focus.x += before.x - after.x;
            m_focus.z += before.z - after.z;
        }
        m_panX = m_window.GetMouseX();
        m_panY = m_window.GetMouseY();
        if (!m_window.IsMouseButtonDown(MouseButton::Middle))
            m_panning = false;
    }

    if (mouseAvailable || m_orbiting)
    {
        const float wheel = m_window.GetMouseWheelDelta();
        if (wheel != 0.0f)
            m_distance = std::clamp(m_distance * std::pow(kZoomPerNotch, -wheel), kMinDistance, kMaxDistance);
    }

    if (keyboardAvailable)
    {
        float turn = 0.0f;
        if (m_window.IsKeyDown(Key::Q)) turn -= 1.0f;
        if (m_window.IsKeyDown(Key::E)) turn += 1.0f;
        m_yaw += turn * kKeyTurnSpeed * deltaSeconds;

        Vec2 dir;
        if (m_window.IsKeyDown(Key::Left) || m_window.IsKeyDown(Key::A))  dir.x -= 1.0f;
        if (m_window.IsKeyDown(Key::Right) || m_window.IsKeyDown(Key::D)) dir.x += 1.0f;
        if (m_window.IsKeyDown(Key::Up) || m_window.IsKeyDown(Key::W))    dir.y += 1.0f;
        if (m_window.IsKeyDown(Key::Down) || m_window.IsKeyDown(Key::S))  dir.y -= 1.0f;
        if (dir.x != 0.0f && dir.y != 0.0f)
            dir = dir * 0.70710678f; // same speed diagonally

        // Pan in view space: screen right / up are the camera's right / forward on the ground.
        const float lens  = std::tan(m_camera.GetVerticalFov() * 0.5f) / std::tan(DegToRad(30.0f)); // a narrow lens shows less
        const float speed = kPanSpeed * lens * m_distance * (m_window.IsKeyDown(Key::Shift) ? kFastPanScale : 1.0f);
        const float sinYaw = std::sin(m_yaw), cosYaw = std::cos(m_yaw);
        const Vec2  right   = { cosYaw, -sinYaw };
        const Vec2  forward = { sinYaw, cosYaw };
        m_focus.x += (dir.x * right.x + dir.y * forward.x) * speed * deltaSeconds;
        m_focus.z += (dir.x * right.y + dir.y * forward.y) * speed * deltaSeconds;
    }

    UpdateCamera();
}

void OrbitCamera::UpdateCamera()
{
    m_focus.x = std::clamp(m_focus.x, m_boundsMin.x, m_boundsMax.x);
    m_focus.z = std::clamp(m_focus.z, m_boundsMin.y, m_boundsMax.y);
    m_focus.y = m_groundHeight;

    m_camera.SetOrientation(m_yaw, m_pitch);
    m_yaw = m_camera.GetYaw(); // wrapped
    m_camera.SetPosition(m_focus - m_camera.GetForward() * m_distance);
}

void OrbitCamera::ScreenRay(Vec2 screen, Vec3& origin, Vec3& direction) const
{
    const float width  = (float)std::max(m_window.GetWidth(), 1u);
    const float height = (float)std::max(m_window.GetHeight(), 1u);
    const float ndcX   = 2.0f * screen.x / width - 1.0f;
    const float ndcY   = 1.0f - 2.0f * screen.y / height;

    const float tanHalfFov = std::tan(m_camera.GetVerticalFov() * 0.5f);
    origin    = m_camera.GetPosition();
    direction = m_camera.GetForward() + m_camera.GetRight() * (ndcX * tanHalfFov * m_camera.GetAspect()) +
                m_camera.GetUp() * (ndcY * tanHalfFov);
}

bool OrbitCamera::ScreenToGround(Vec2 screen, Vec3& ground) const
{
    Vec3 origin, dir;
    ScreenRay(screen, origin, dir);
    if (dir.y >= -1e-5f)
        return false;
    ground = origin + dir * ((m_groundHeight - origin.y) / dir.y);
    return true;
}

bool OrbitCamera::WorldToScreen(Vec3 world, Vec2& screen) const
{
    const Mat4  m = m_camera.GetViewProjection();
    const float x = m.m[0][0] * world.x + m.m[0][1] * world.y + m.m[0][2] * world.z + m.m[0][3];
    const float y = m.m[1][0] * world.x + m.m[1][1] * world.y + m.m[1][2] * world.z + m.m[1][3];
    const float w = m.m[3][0] * world.x + m.m[3][1] * world.y + m.m[3][2] * world.z + m.m[3][3];
    if (w <= 1e-4f)
        return false;
    screen = { (x / w * 0.5f + 0.5f) * (float)m_window.GetWidth(), (0.5f - y / w * 0.5f) * (float)m_window.GetHeight() };
    return true;
}

#if defined(GAME_IMGUI_ENABLED)
void OrbitCamera::DrawCheats()
{
    ImGui::TextUnformatted("WASD / arrows: pan (Shift: faster)  Wheel: zoom");
    ImGui::TextUnformatted("Q/E: turn  Hold RMB + drag: turn / tilt");
    ImGui::DragFloat3("Focus", &m_focus.x, 0.1f);
    ImGui::SliderFloat("Distance", &m_distance, kMinDistance, kMaxDistance);
    float yawDeg = RadToDeg(m_yaw);
    if (ImGui::SliderFloat("Yaw", &yawDeg, -180.0f, 180.0f, "%.0f deg"))
        m_yaw = DegToRad(yawDeg);
    float pitchDeg = RadToDeg(-m_pitch);
    if (ImGui::SliderFloat("Pitch", &pitchDeg, RadToDeg(-kMaxPitch), RadToDeg(-kMinPitch), "%.0f deg"))
        m_pitch = -DegToRad(pitchDeg);
    if (ImGui::Button("Reset"))
        Reset({});
    UpdateCamera();
}
#endif
