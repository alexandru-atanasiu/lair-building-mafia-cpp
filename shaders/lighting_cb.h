// Per-scope constants of the mesh and skinned mesh renderers (shaders/lighting.hlsli, filled
// by src/render/lighting_constants.h). Shared by HLSL and C++, see cbuffer.h.

#ifndef LIGHTING_CB_H
#define LIGHTING_CB_H

#include "cbuffer.h"

CBUFFER_BEGIN(LightingConstants, b0)
    CB_FLOAT4X4(viewProj);
    CB_FLOAT3(cameraPosition);
    CB_FLOAT(pad0);
    CB_FLOAT3(lightPosition);
    CB_FLOAT(lightRange);
    CB_FLOAT3(lightColor);    // color * intensity
    CB_FLOAT(specular);
    CB_FLOAT3(ambient);
    CB_FLOAT(shininess);
    CB_FLOAT3(directionalToLight); // normalized, toward the light
    CB_FLOAT(pad1);
    CB_FLOAT3(directionalColor);   // color * intensity
    CB_FLOAT(pad2);
CBUFFER_END

#endif
