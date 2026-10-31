#pragma once

#include "core/color.h"
#include "core/math.h"

// Scene lighting shared by the 3D renderers (MeshRenderer, SkinnedMeshRenderer).

// Light from infinitely far away (e.g. the sun): same direction everywhere, no falloff.
struct DirectionalLight
{
    Vec3  direction = { 0.0f, -1.0f, 0.0f }; // direction the light travels (normalized when used)
    Color color;                             // rgb used, alpha ignored
    float intensity = 0.0f;                  // off by default
};

struct PointLight
{
    Vec3  position;
    Color color;              // rgb used, alpha ignored
    float intensity = 1.0f;
    float range     = 30.0f;  // no light past this distance (smooth falloff to it)
};

struct MeshLighting
{
    DirectionalLight directional;
    PointLight       light;
    Color            ambient   = { 0.12f, 0.12f, 0.15f, 1.0f };
    float            specular  = 0.4f;  // strength of the white highlight
    float            shininess = 32.0f; // Blinn-Phong exponent
};
