#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class IBuffer;
class IPipeline;
class ITexture;
class IWindow;

enum class VertexFormat
{
    Float1,
    Float2,
    Float3,
    Float4,
    UNorm8x4, // 4 bytes, read as float4 in [0, 1] (e.g. a packed RGBA color)
    UInt8x4,  // 4 bytes, read as uint4 (e.g. joint indices)
    UInt1,    // 32-bit uint
};

// How the pipeline's single vertex buffer is stepped through.
enum class VertexRate
{
    PerVertex,
    PerInstance, // one element per instance; per-vertex data then comes from SV_VertexID
};

enum class PrimitiveTopology
{
    TriangleList,
    TriangleStrip,
};

enum class BlendMode
{
    Opaque,
    PremultipliedAlpha, // dst = src + dst * (1 - src.a)
};

enum class CullMode
{
    Back, // clockwise triangles are front-facing
    None,
};

// Filter of the sampler bound to s0 of the pixel shader. Addressing is always clamp.
enum class TextureFilter
{
    Linear, // trilinear
    Point,
};

// The back buffer has a 32-bit float depth buffer, cleared to 1 (far) by BeginFrame.
enum class DepthMode
{
    Off,       // no test, no write
    ReadWrite, // less-than test, writes depth
    ReadOnly,  // less-than test, no write
};

struct VertexAttribute
{
    const char*  semantic;
    VertexFormat format;
    uint32_t     offset;   // bytes from the start of the element
    uint32_t     slot = 0; // 0 = vertex stream (DrawDesc::vertexBuffer), 1 = instance stream
};

// Precompiled shader bytecode (as produced by cook.bat). Only needs to stay valid
// for the duration of CreatePipeline.
struct ShaderBytecode
{
    const void* data;
    size_t      size;
};

struct PipelineDesc
{
    ShaderBytecode         vertexShader;
    ShaderBytecode         pixelShader;
    const VertexAttribute* attributes;
    uint32_t               attributeCount;
    uint32_t               vertexStride;              // stream 0
    VertexRate             vertexRate     = VertexRate::PerVertex;
    uint32_t               instanceStride = 0;        // stream 1, always per instance (0 = unused)
    PrimitiveTopology      topology       = PrimitiveTopology::TriangleList;
    BlendMode              blend          = BlendMode::Opaque;
    CullMode               cull           = CullMode::Back;
    TextureFilter          filter         = TextureFilter::Linear;
    DepthMode              depth          = DepthMode::Off;
};

enum class TextureFormat
{
    RGBA8Unorm,
};

struct TextureDesc
{
    uint32_t      width;
    uint32_t      height;
    uint32_t      mipCount = 1;
    TextureFormat format   = TextureFormat::RGBA8Unorm;
    // All mips back to back, largest first; mip i is max(1, width >> i) x max(1, height >> i)
    // pixels with tightly packed rows, top to bottom (the layout of cooked .tex files).
    const void*   data     = nullptr;
};

// Everything a draw needs. Instance/index/constant buffers and texture are optional.
struct DrawDesc
{
    const IPipeline* pipeline       = nullptr;
    const IBuffer*   vertexBuffer   = nullptr; // stream 0: per vertex or per instance, as the pipeline says
    const IBuffer*   instanceBuffer = nullptr; // stream 1, if the pipeline has an instance stride
    const IBuffer*   indexBuffer    = nullptr; // 32-bit indices; draws indexCount indices if set
    const IBuffer*   constantBuffer = nullptr; // b0 of both shader stages
    const IBuffer*   vertexShaderBuffer = nullptr; // structured buffer at t0 of the vertex shader
    const ITexture*  texture        = nullptr; // t0 of the pixel shader, sampled with s0
    uint32_t         vertexCount    = 0;       // per instance, when not indexed
    uint32_t         indexCount     = 0;       // per instance, when indexed
    uint32_t         instanceCount  = 1;
    uint32_t         firstVertex    = 0;
    uint32_t         firstIndex     = 0;
    uint32_t         firstInstance  = 0;
};

class IBuffer
{
public:
    virtual ~IBuffer() = default;
    virtual uint32_t GetSizeBytes() const = 0;
};

class ITexture
{
public:
    virtual ~ITexture() = default;
    virtual uint32_t GetWidth() const  = 0;
    virtual uint32_t GetHeight() const = 0;
};

class IPipeline
{
public:
    virtual ~IPipeline() = default;
    virtual uint32_t GetVertexStride() const = 0;
};

class IRenderDevice
{
public:
    virtual ~IRenderDevice() = default;

    // Immutable vertex buffer.
    virtual std::unique_ptr<IBuffer>   CreateVertexBuffer(const void* data, uint32_t sizeBytes) = 0;
    // Immutable index buffer of 32-bit indices.
    virtual std::unique_ptr<IBuffer>   CreateIndexBuffer(const uint32_t* indices, uint32_t indexCount) = 0;
    // CPU-writable buffers, filled with UpdateBuffer. Constant buffer sizes must be multiples of 16.
    virtual std::unique_ptr<IBuffer>   CreateDynamicVertexBuffer(uint32_t sizeBytes) = 0;
    virtual std::unique_ptr<IBuffer>   CreateConstantBuffer(uint32_t sizeBytes) = 0;
    // Shader-readable array of `elementCount` structs (HLSL StructuredBuffer<T>), CPU-writable.
    virtual std::unique_ptr<IBuffer>   CreateDynamicStructuredBuffer(uint32_t elementSize, uint32_t elementCount) = 0;
    virtual std::unique_ptr<ITexture>  CreateTexture(const TextureDesc& desc) = 0;
    virtual std::unique_ptr<IPipeline> CreatePipeline(const PipelineDesc& desc) = 0;

    // Replaces the start of a dynamic buffer's contents; the rest becomes undefined. Draws
    // already submitted keep seeing the old contents, so a buffer can be refilled per draw.
    virtual void UpdateBuffer(IBuffer& buffer, const void* data, uint32_t sizeBytes) = 0;

    // Resizes the back buffer. No-op if the size is unchanged or zero.
    virtual void Resize(uint32_t width, uint32_t height) = 0;

    // Clears the back buffer to `clearColor` and the depth buffer to 1.
    virtual void BeginFrame(const float clearColor[4]) = 0;
    virtual void Draw(const DrawDesc& desc) = 0;
    void Draw(const IPipeline& pipeline, const IBuffer& vertexBuffer, uint32_t vertexCount)
    {
        DrawDesc desc;
        desc.pipeline     = &pipeline;
        desc.vertexBuffer = &vertexBuffer;
        desc.vertexCount  = vertexCount;
        Draw(desc);
    }
    virtual void EndFrame() = 0; // presents

#if defined(GAME_IMGUI_ENABLED)
    // Dear ImGui renderer backend. Driven by ImGuiLayer; requires a current ImGui context.
    virtual void ImGuiInitBackend()     = 0;
    virtual void ImGuiShutdownBackend() = 0;
    virtual void ImGuiNewFrame()        = 0;
    // Draws ImGui's current draw data into the back buffer. Call between BeginFrame and EndFrame.
    virtual void ImGuiRender()          = 0;
#endif

#if defined(GAME_DEV_TOOLS_ENABLED)
    // Reads the back buffer as RGBA8 rows, top to bottom (opaque). Call before EndFrame.
    virtual bool CaptureBackBuffer(std::vector<uint8_t>& rgba, uint32_t& width, uint32_t& height) = 0;
#endif
};

struct RenderDeviceDesc
{
    // Wait for the display's vertical blank on present. When off, frames are presented
    // immediately (with tearing, where the system supports it).
    bool vsync = true;
};

std::unique_ptr<IRenderDevice> CreateRenderDevice(IWindow& window, const RenderDeviceDesc& desc);
