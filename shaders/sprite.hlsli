// Shared between sprite.vs.hlsl and sprite.ps.hlsl.

// One sprite per instance (SpriteBatch::Instance).
struct VSIn
{
    float2 center   : CENTER;   // world space
    float2 halfSize : HALFSIZE;
    float  rotation : ROTATION; // radians, counter-clockwise
    float4 uvRect   : UVRECT;   // u0, v0, u1, v1
    float4 color    : COLOR;    // straight alpha
};

struct VSOut
{
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR;       // premultiplied alpha
};
