#include "skinned.hlsli"

float4 main(VSOut i) : SV_Target
{
    return float4(i.color, 1.0);
}
