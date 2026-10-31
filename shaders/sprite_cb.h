// Camera constants of the sprite batch (shaders/sprite.vs.hlsl, src/render/sprite_batch.cpp).
// Shared by HLSL and C++, see cbuffer.h.

#ifndef SPRITE_CB_H
#define SPRITE_CB_H

#include "cbuffer.h"

CBUFFER_BEGIN(SpriteCameraConstants, b0)
    CB_FLOAT2(worldToClipScale);
    CB_FLOAT2(worldToClipOffset);
CBUFFER_END

#endif
