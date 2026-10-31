// Shared between mesh.vs.hlsl and mesh.ps.hlsl.

struct VSIn
{
    // Stream 0: MeshVertex.
    float3 pos      : POSITION;
    float3 normal   : NORMAL;
    // Stream 1: MeshRenderer::Instance.
    float4 worldX   : WORLDX;   // rows of the world matrix
    float4 worldY   : WORLDY;
    float4 worldZ   : WORLDZ;
    float4 color    : COLOR;
    float  emissive : EMISSIVE;
};

struct VSOut
{
    float4 pos   : SV_Position;
    float3 color : COLOR;       // lit in the vertex shader, interpolated (Gouraud)
};
