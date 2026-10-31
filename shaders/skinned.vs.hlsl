#include "skinned.hlsli"
#include "lighting.hlsli"

// SkinnedMeshRenderer::JointMatrix: skinning matrix (bind pose -> world), top 3 rows.
struct JointMatrix
{
    float4 row0;
    float4 row1;
    float4 row2;
};
StructuredBuffer<JointMatrix> palette : register(t0);

float3x4 LoadJoint(uint index)
{
    const JointMatrix j = palette[index];
    return float3x4(j.row0, j.row1, j.row2);
}

// Linear blend skinning with up to 4 joints, then Gouraud lighting (per vertex).
VSOut main(VSIn i)
{
    const uint     base = i.paletteOffset;
    const float3x4 skin = LoadJoint(base + i.joints.x) * i.weights.x +
                          LoadJoint(base + i.joints.y) * i.weights.y +
                          LoadJoint(base + i.joints.z) * i.weights.z +
                          LoadJoint(base + i.joints.w) * i.weights.w;

    const float3 worldPos = mul(skin, float4(i.pos, 1.0));
    const float3 n        = TransformNormal(skin, i.normal);

    VSOut o;
    o.pos   = mul(viewProj, float4(worldPos, 1.0));
    o.color = ShadeVertex(worldPos, n, i.material.rgb * i.color.rgb, 0.0);
    return o;
}
