// Shared between skinned.vs.hlsl and skinned.ps.hlsl.

struct VSIn
{
    // Stream 0: SkinnedVertex.
    float3 pos           : POSITION;
    float3 normal        : NORMAL;
    uint4  joints        : JOINTS;
    float4 weights       : WEIGHTS;
    float4 material      : MATERIAL; // vertex color from the mesh's materials
    // Stream 1: SkinnedMeshRenderer::Instance.
    uint   paletteOffset : PALETTE;
    float4 color         : COLOR;
};

struct VSOut
{
    float4 pos   : SV_Position;
    float3 color : COLOR;       // lit in the vertex shader, interpolated (Gouraud)
};
