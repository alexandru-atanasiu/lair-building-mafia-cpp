#include "mesh.hlsli"
#include "lighting.hlsli"

// Gouraud shading: lighting runs per vertex and the rasterizer interpolates the resulting
// color across each triangle.
VSOut main(VSIn i)
{
    const float3x4 world    = float3x4(i.worldX, i.worldY, i.worldZ);
    const float3   worldPos = mul(world, float4(i.pos, 1.0));
    const float3   n        = TransformNormal(world, i.normal);

    VSOut o;
    o.pos   = mul(viewProj, float4(worldPos, 1.0));
    o.color = float4(ShadeVertex(worldPos, n, i.color.rgb, i.emissive) * i.color.a, i.color.a);
    return o;
}
