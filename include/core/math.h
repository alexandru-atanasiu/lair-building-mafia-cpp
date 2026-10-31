#pragma once

#include <cmath>

// Vectors and matrices. 3D follows the engine's conventions: left-handed, Y-up, +Z forward.
// Matrices use column vectors (p' = M * p) and are stored row-major: m[row][col], translation
// in column 3. HLSL declares them `row_major` and multiplies with mul(M, v).

constexpr float kPi = 3.14159265358979f;

constexpr float DegToRad(float degrees) { return degrees * (kPi / 180.0f); }
constexpr float RadToDeg(float radians) { return radians * (180.0f / kPi); }

// ---------------------------------------------------------------------------
// Vec2
// ---------------------------------------------------------------------------

struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;
};

constexpr Vec2 operator+(Vec2 a, Vec2 b) { return { a.x + b.x, a.y + b.y }; }
constexpr Vec2 operator-(Vec2 a, Vec2 b) { return { a.x - b.x, a.y - b.y }; }
constexpr Vec2 operator*(Vec2 v, float s) { return { v.x * s, v.y * s }; }
constexpr Vec2 operator*(float s, Vec2 v) { return { v.x * s, v.y * s }; }
constexpr Vec2 operator/(Vec2 v, float s) { return { v.x / s, v.y / s }; }

constexpr Vec2& operator+=(Vec2& a, Vec2 b) { a.x += b.x; a.y += b.y; return a; }
constexpr Vec2& operator-=(Vec2& a, Vec2 b) { a.x -= b.x; a.y -= b.y; return a; }

// ---------------------------------------------------------------------------
// Vec3 / Mat4
// ---------------------------------------------------------------------------

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

constexpr Vec3 operator+(Vec3 a, Vec3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
constexpr Vec3 operator-(Vec3 a, Vec3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
constexpr Vec3 operator-(Vec3 v) { return { -v.x, -v.y, -v.z }; }
constexpr Vec3 operator*(Vec3 v, float s) { return { v.x * s, v.y * s, v.z * s }; }
constexpr Vec3 operator*(float s, Vec3 v) { return { v.x * s, v.y * s, v.z * s }; }

constexpr Vec3& operator+=(Vec3& a, Vec3 b) { a.x += b.x; a.y += b.y; a.z += b.z; return a; }
constexpr Vec3& operator-=(Vec3& a, Vec3 b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; return a; }

constexpr float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr Vec3  Cross(Vec3 a, Vec3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
inline float    Length(Vec3 v) { return std::sqrt(Dot(v, v)); }
inline Vec3     Normalize(Vec3 v)
{
    const float len = Length(v);
    return len > 0.0f ? v * (1.0f / len) : Vec3{};
}

struct Mat4
{
    float m[4][4] = {};

    static constexpr Mat4 Identity()
    {
        Mat4 r;
        r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.0f;
        return r;
    }

    static constexpr Mat4 Translation(Vec3 t)
    {
        Mat4 r = Identity();
        r.m[0][3] = t.x;
        r.m[1][3] = t.y;
        r.m[2][3] = t.z;
        return r;
    }

    static constexpr Mat4 Scale(Vec3 s)
    {
        Mat4 r;
        r.m[0][0] = s.x;
        r.m[1][1] = s.y;
        r.m[2][2] = s.z;
        r.m[3][3] = 1.0f;
        return r;
    }

    // Rotations by `radians`, clockwise when looking down the axis toward the origin
    // (left-handed): RotationY turns +Z toward +X.
    static Mat4 RotationX(float radians)
    {
        const float s = std::sin(radians), c = std::cos(radians);
        Mat4 r = Identity();
        r.m[1][1] = c;  r.m[1][2] = -s;
        r.m[2][1] = s;  r.m[2][2] = c;
        return r;
    }

    static Mat4 RotationY(float radians)
    {
        const float s = std::sin(radians), c = std::cos(radians);
        Mat4 r = Identity();
        r.m[0][0] = c;  r.m[0][2] = s;
        r.m[2][0] = -s; r.m[2][2] = c;
        return r;
    }

    static Mat4 RotationZ(float radians)
    {
        const float s = std::sin(radians), c = std::cos(radians);
        Mat4 r = Identity();
        r.m[0][0] = c;  r.m[0][1] = -s;
        r.m[1][0] = s;  r.m[1][1] = c;
        return r;
    }

    // Translation * Rotation * Scale.
    static Mat4 TRS(Vec3 translation, const Mat4& rotation, Vec3 scale)
    {
        return Translation(translation) * rotation * Scale(scale);
    }

    // View matrix of a camera at `eye` with orthonormal basis right/up/forward.
    static constexpr Mat4 View(Vec3 eye, Vec3 right, Vec3 up, Vec3 forward)
    {
        Mat4 r;
        const Vec3 axes[3] = { right, up, forward };
        for (int i = 0; i < 3; ++i)
        {
            r.m[i][0] = axes[i].x;
            r.m[i][1] = axes[i].y;
            r.m[i][2] = axes[i].z;
            r.m[i][3] = -Dot(axes[i], eye);
        }
        r.m[3][3] = 1.0f;
        return r;
    }

    // Left-handed perspective projection, depth mapped to [0, 1] between near and far.
    static Mat4 PerspectiveFov(float verticalFovRadians, float aspect, float nearZ, float farZ)
    {
        const float yScale = 1.0f / std::tan(verticalFovRadians * 0.5f);
        Mat4 r;
        r.m[0][0] = yScale / aspect;
        r.m[1][1] = yScale;
        r.m[2][2] = farZ / (farZ - nearZ);
        r.m[2][3] = -nearZ * farZ / (farZ - nearZ);
        r.m[3][2] = 1.0f;
        return r;
    }

    friend constexpr Mat4 operator*(const Mat4& a, const Mat4& b)
    {
        Mat4 r;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
        return r;
    }

    // a * b for affine matrices (bottom row 0, 0, 0, 1): only the top 3 rows are computed,
    // about half the work of the full product.
    friend constexpr Mat4 MulAffine(const Mat4& a, const Mat4& b)
    {
        Mat4 r;
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 4; ++j)
                r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j];
            r.m[i][3] += a.m[i][3];
        }
        r.m[3][3] = 1.0f;
        return r;
    }

    constexpr Vec3 TransformPoint(Vec3 p) const
    {
        return { m[0][0] * p.x + m[0][1] * p.y + m[0][2] * p.z + m[0][3],
                 m[1][0] * p.x + m[1][1] * p.y + m[1][2] * p.z + m[1][3],
                 m[2][0] * p.x + m[2][1] * p.y + m[2][2] * p.z + m[2][3] };
    }

    // Length of the longest transformed basis axis (bounding sphere scaling).
    float MaxAxisScale() const
    {
        float maxSq = 0.0f;
        for (int col = 0; col < 3; ++col)
        {
            const float sq = m[0][col] * m[0][col] + m[1][col] * m[1][col] + m[2][col] * m[2][col];
            maxSq = sq > maxSq ? sq : maxSq;
        }
        return std::sqrt(maxSq);
    }
};
