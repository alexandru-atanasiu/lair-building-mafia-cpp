#include "sprite.hlsli"
#include "sprite_cb.h"

// Drawn as a 4-vertex triangle strip per instance:
// vertex 0 = top-left, 1 = top-right, 2 = bottom-left, 3 = bottom-right.
VSOut main(VSIn i, uint vertexId : SV_VertexID)
{
    const float2 corner = float2((float)(vertexId & 1), (float)(vertexId >> 1));

    // World is y-up, so the top row (corner.y = 0) is at +halfSize.y.
    const float2 local = float2(corner.x * 2.0 - 1.0, 1.0 - corner.y * 2.0) * i.halfSize;

    float s, c;
    sincos(i.rotation, s, c);
    const float2 world = i.center + float2(local.x * c - local.y * s, local.x * s + local.y * c);

    VSOut o;
    o.pos   = float4(world * worldToClipScale + worldToClipOffset, 0.0, 1.0);
    o.uv    = lerp(i.uvRect.xy, i.uvRect.zw, corner);
    o.color = float4(i.color.rgb * i.color.a, i.color.a);
    return o;
}
