// Per-vertex (Gouraud) lighting shared by the mesh and skinned mesh vertex shaders.

#include "lighting_cb.h"

// Normals transform by the inverse transpose of the 3x3 part of `m`. Its cofactor matrix is the
// same up to a scale (removed by normalize) and the determinant's sign (mirroring).
float3 TransformNormal(float3x4 m, float3 normal)
{
    const float3 c0   = float3(m[0].x, m[1].x, m[2].x);
    const float3 c1   = float3(m[0].y, m[1].y, m[2].y);
    const float3 c2   = float3(m[0].z, m[1].z, m[2].z);
    const float3 cof0 = cross(c1, c2);
    const float3 cof1 = cross(c2, c0);
    const float3 cof2 = cross(c0, c1);
    const float  det  = dot(c0, cof0);
    return normalize((cof0 * normal.x + cof1 * normal.y + cof2 * normal.z) * (det < 0.0 ? -1.0 : 1.0));
}

// Lambert diffuse + Blinn-Phong specular of one light of `radiance` coming from direction `l`
// (normalized, toward the light), added to `diffuse` and `highlight`.
void AddLight(float3 n, float3 v, float3 l, float3 radiance, inout float3 diffuse, inout float3 highlight)
{
    const float  nDotL = saturate(dot(n, l));
    const float3 h     = normalize(l + v);
    const float  spec  = nDotL > 0.0 ? pow(max(dot(n, h), 1e-6), shininess) : 0.0;
    diffuse   += radiance * nDotL;
    highlight += radiance * (spec * specular);
}

// One directional light, one point light with a smooth falloff to zero at lightRange, ambient,
// Blinn-Phong specular. `emissive` blends from lit (0) to the plain albedo (1).
float3 ShadeVertex(float3 worldPos, float3 n, float3 albedo, float emissive)
{
    const float3 v = normalize(cameraPosition - worldPos);

    float3 diffuse   = ambient;
    float3 highlight = 0.0;
    AddLight(n, v, directionalToLight, directionalColor, diffuse, highlight);

    const float3 toLight = lightPosition - worldPos;
    const float  dist    = length(toLight);
    float        falloff = saturate(1.0 - (dist * dist) / (lightRange * lightRange));
    falloff *= falloff;
    AddLight(n, v, toLight / max(dist, 1e-4), lightColor * falloff, diffuse, highlight);

    const float3 lit = albedo * diffuse + highlight;
    return lerp(lit, albedo, emissive);
}
