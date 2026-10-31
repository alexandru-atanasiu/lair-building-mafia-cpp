#include "sprite.hlsli"

// Premultiplied alpha (cooked by py/pack_texture.py).
Texture2D    spriteTexture : register(t0);
SamplerState spriteSampler : register(s0);

float4 main(VSOut i) : SV_Target
{
    return spriteTexture.Sample(spriteSampler, i.uv) * i.color;
}
