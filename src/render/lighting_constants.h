#pragma once

#include "core/math.h"
#include "render/lighting.h"

#include <algorithm>

// LightingConstants: cbuffer of shaders/lighting.hlsli, declared once for HLSL and C++.
#include "../../shaders/lighting_cb.h"

static_assert(sizeof(LightingConstants) == 160);

inline LightingConstants MakeLightingConstants(const Mat4& viewProj, Vec3 cameraPosition, const MeshLighting& lighting)
{
    LightingConstants c = {};
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            c.viewProj[row][col] = viewProj.m[row][col];

    const PointLight& light = lighting.light;
    c.cameraPosition[0] = cameraPosition.x;
    c.cameraPosition[1] = cameraPosition.y;
    c.cameraPosition[2] = cameraPosition.z;
    c.lightPosition[0]  = light.position.x;
    c.lightPosition[1]  = light.position.y;
    c.lightPosition[2]  = light.position.z;
    c.lightRange        = std::max(light.range, 1e-3f);
    c.lightColor[0]     = light.color.r * light.intensity;
    c.lightColor[1]     = light.color.g * light.intensity;
    c.lightColor[2]     = light.color.b * light.intensity;
    c.specular          = lighting.specular;
    c.ambient[0]        = lighting.ambient.r;
    c.ambient[1]        = lighting.ambient.g;
    c.ambient[2]        = lighting.ambient.b;
    c.shininess         = std::max(lighting.shininess, 1.0f);

    const DirectionalLight& directional = lighting.directional;
    const Vec3              toLight     = Normalize(-directional.direction);
    c.directionalToLight[0] = toLight.x;
    c.directionalToLight[1] = toLight.y;
    c.directionalToLight[2] = toLight.z;
    c.directionalColor[0]   = directional.color.r * directional.intensity;
    c.directionalColor[1]   = directional.color.g * directional.intensity;
    c.directionalColor[2]   = directional.color.b * directional.intensity;
    return c;
}
