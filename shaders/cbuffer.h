// Declares a constant buffer once for both HLSL and C++ (included by both).
//
//   CBUFFER_BEGIN(Name, b0)
//       CB_FLOAT4X4(viewProj);
//       CB_FLOAT3(position);
//       CB_FLOAT(radius);
//   CBUFFER_END
//
// HLSL gets `cbuffer Name : register(b0)`, C++ `struct Name`. The memory layouts match as long
// as members never straddle a 16-byte register (HLSL packing): pad float3/float2 with floats so
// each register is filled, and static_assert the size on the C++ side. Matrices are row_major.

#ifndef CBUFFER_H
#define CBUFFER_H

#if defined(__cplusplus)

#define CBUFFER_BEGIN(name, reg) struct name {
#define CBUFFER_END              };
#define CB_FLOAT4X4(n)           float n[4][4]
#define CB_FLOAT4(n)             float n[4]
#define CB_FLOAT3(n)             float n[3]
#define CB_FLOAT2(n)             float n[2]
#define CB_FLOAT(n)              float n

#else

#define CBUFFER_BEGIN(name, reg) cbuffer name : register(reg) {
#define CBUFFER_END              };
#define CB_FLOAT4X4(n)           row_major float4x4 n
#define CB_FLOAT4(n)             float4 n
#define CB_FLOAT3(n)             float3 n
#define CB_FLOAT2(n)             float2 n
#define CB_FLOAT(n)              float n

#endif

#endif
