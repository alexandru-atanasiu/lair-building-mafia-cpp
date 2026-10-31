#include "d3d11_render_device.h"

#include "core/assert.h"
#include "core/fatal.h"
#include "core/log.h"
#include "platform/window.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#if defined(GAME_IMGUI_ENABLED)
#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#endif

using Microsoft::WRL::ComPtr;

static constexpr DXGI_FORMAT kBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
static constexpr DXGI_FORMAT kDepthFormat      = DXGI_FORMAT_D32_FLOAT;

#define CHECK_HR(expr, msg)                \
    do {                                   \
        HRESULT hr_ = (expr);              \
        if (FAILED(hr_)) FailHr(msg, hr_); \
    } while (0)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static DXGI_FORMAT ToDxgiFormat(VertexFormat format)
{
    switch (format)
    {
    case VertexFormat::Float1:   return DXGI_FORMAT_R32_FLOAT;
    case VertexFormat::Float2:   return DXGI_FORMAT_R32G32_FLOAT;
    case VertexFormat::Float3:   return DXGI_FORMAT_R32G32B32_FLOAT;
    case VertexFormat::Float4:   return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case VertexFormat::UNorm8x4: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case VertexFormat::UInt8x4:  return DXGI_FORMAT_R8G8B8A8_UINT;
    case VertexFormat::UInt1:    return DXGI_FORMAT_R32_UINT;
    }
    Fatal("Unknown VertexFormat");
}

static DXGI_FORMAT ToDxgiFormat(TextureFormat format)
{
    switch (format)
    {
    case TextureFormat::RGBA8Unorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    Fatal("Unknown TextureFormat");
}

static uint32_t BytesPerPixel(TextureFormat format)
{
    switch (format)
    {
    case TextureFormat::RGBA8Unorm: return 4;
    }
    Fatal("Unknown TextureFormat");
}

static const char* DeviceRemovedReasonName(HRESULT reason)
{
    switch (reason)
    {
    case DXGI_ERROR_DEVICE_HUNG:                 return "DXGI_ERROR_DEVICE_HUNG (the GPU took too long on a command)";
    case DXGI_ERROR_DEVICE_REMOVED:              return "DXGI_ERROR_DEVICE_REMOVED (GPU removed, or driver updated)";
    case DXGI_ERROR_DEVICE_RESET:                return "DXGI_ERROR_DEVICE_RESET (bad commands reset the GPU)";
    case DXGI_ERROR_DRIVER_INTERNAL_ERROR:       return "DXGI_ERROR_DRIVER_INTERNAL_ERROR";
    case DXGI_ERROR_INVALID_CALL:                return "DXGI_ERROR_INVALID_CALL";
    case S_OK:                                   return "S_OK (device not removed)";
    }
    return "unknown reason";
}

static D3D11_PRIMITIVE_TOPOLOGY ToD3DTopology(PrimitiveTopology topology)
{
    switch (topology)
    {
    case PrimitiveTopology::TriangleList:  return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case PrimitiveTopology::TriangleStrip: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    }
    Fatal("Unknown PrimitiveTopology");
}

// ---------------------------------------------------------------------------
// Resources
// ---------------------------------------------------------------------------

D3D11Buffer::D3D11Buffer(ComPtr<ID3D11Buffer> buffer, uint32_t sizeBytes, bool dynamic, ComPtr<ID3D11ShaderResourceView> srv)
    : m_buffer(std::move(buffer))
    , m_sizeBytes(sizeBytes)
    , m_dynamic(dynamic)
    , m_srv(std::move(srv))
{
}

D3D11Texture::D3D11Texture(ComPtr<ID3D11ShaderResourceView> srv, uint32_t width, uint32_t height)
    : m_srv(std::move(srv))
    , m_width(width)
    , m_height(height)
{
}

D3D11Pipeline::D3D11Pipeline(ComPtr<ID3D11VertexShader> vs,
                             ComPtr<ID3D11PixelShader>  ps,
                             ComPtr<ID3D11InputLayout>  inputLayout,
                             D3D11PipelineState         state,
                             uint32_t                   vertexStride)
    : m_vs(std::move(vs))
    , m_ps(std::move(ps))
    , m_inputLayout(std::move(inputLayout))
    , m_state(std::move(state))
    , m_vertexStride(vertexStride)
{
}

// ---------------------------------------------------------------------------
// Device
// ---------------------------------------------------------------------------

D3D11RenderDevice::D3D11RenderDevice(HWND hwnd, uint32_t width, uint32_t height, const RenderDeviceDesc& desc)
    : m_width(width)
    , m_height(height)
    , m_vsync(desc.vsync)
{
    UINT deviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(GAME_DBG)
    deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };

    D3D_FEATURE_LEVEL featureLevel = {};
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, deviceFlags,
                                   levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
                                   &m_device, &featureLevel, &m_context);
#if defined(GAME_DBG)
    if (FAILED(hr))
    {
        // Debug layer missing (Graphics Tools optional feature not installed) - retry without it.
        LOG_WARN("D3D11 debug layer unavailable (0x%08X), creating device without it", (unsigned)hr);
        deviceFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, deviceFlags,
                               levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
                               &m_device, &featureLevel, &m_context);
    }
#endif
    CHECK_HR(hr, "D3D11CreateDevice failed");
    m_debugLayer = (deviceFlags & D3D11_CREATE_DEVICE_DEBUG) != 0;

    // Under a debugger, stop at the call that caused a debug layer error (otherwise the
    // message just scrolls by in the output). Without one the break would be a crash.
    if (m_debugLayer && IsDebuggerPresent())
    {
        ComPtr<ID3D11InfoQueue> infoQueue;
        if (SUCCEEDED(m_device.As(&infoQueue)))
        {
            infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
        }
    }

    ComPtr<IDXGIDevice1>  dxgiDevice;
    ComPtr<IDXGIAdapter>  adapter;
    ComPtr<IDXGIFactory2> factory;
    CHECK_HR(m_device.As(&dxgiDevice), "Query IDXGIDevice1 failed");
    // The CPU may only run one frame ahead of the GPU (the default is 3): less input lag.
    CHECK_HR(dxgiDevice->SetMaximumFrameLatency(1), "SetMaximumFrameLatency failed");
    CHECK_HR(dxgiDevice->GetAdapter(&adapter), "GetAdapter failed");
    CHECK_HR(adapter->GetParent(IID_PPV_ARGS(&factory)), "Get IDXGIFactory2 failed");

    DXGI_ADAPTER_DESC adapterDesc = {};
    adapter->GetDesc(&adapterDesc);
    LOG_INFO("D3D11 device created: %ls, feature level %u.%u, debug layer %s",
             adapterDesc.Description, (unsigned)((featureLevel >> 12) & 0xF), (unsigned)((featureLevel >> 8) & 0xF),
             (deviceFlags & D3D11_CREATE_DEVICE_DEBUG) ? "on" : "off");

    // Without vsync (sync interval 0) rendering is uncapped either way. Allowing tearing also
    // lets each frame reach the screen immediately when the window covers the whole monitor;
    // otherwise the compositor shows the latest frame at the next refresh.
    bool tearingSupported = false;
    if (!m_vsync)
    {
        ComPtr<IDXGIFactory5> factory5;
        BOOL                  allowTearing = FALSE;
        if (SUCCEEDED(factory.As(&factory5)) &&
            SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
            tearingSupported = allowTearing != FALSE;
    }
    m_swapChainFlags = tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    m_presentFlags   = tearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0;
    LOG_INFO("VSync %s%s", m_vsync ? "on" : "off",
             m_vsync ? "" : (tearingSupported ? " (tearing allowed)" : " (tearing not supported)"));

    DXGI_SWAP_CHAIN_DESC1 scd = {};
    scd.Width       = width;
    scd.Height      = height;
    scd.Format      = kBackBufferFormat;
    scd.SampleDesc  = { 1, 0 };
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 2;
    scd.Scaling     = DXGI_SCALING_STRETCH;
    scd.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.AlphaMode   = DXGI_ALPHA_MODE_IGNORE;
    scd.Flags       = m_swapChainFlags;
    CHECK_HR(factory->CreateSwapChainForHwnd(m_device.Get(), hwnd, &scd, nullptr, nullptr, &m_swapChain),
             "CreateSwapChainForHwnd failed");

    // Fullscreen is handled by the window (borderless); stop DXGI from hijacking Alt+Enter.
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    CreateBackBufferViews();
}

D3D11RenderDevice::~D3D11RenderDevice()
{
    if (!m_debugLayer)
        return;

    // Everything created through this device should be gone by now (the game is destroyed
    // first): report what's left in the debug output.
    m_context->ClearState();
    m_context->Flush();
    m_rtv.Reset();
    m_dsv.Reset();
    m_swapChain.Reset();

    ComPtr<ID3D11Debug> debug;
    if (SUCCEEDED(m_device.As(&debug)))
        debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
}

void D3D11RenderDevice::FailHr(const char* msg, HRESULT hr) const
{
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        const HRESULT reason = m_device ? m_device->GetDeviceRemovedReason() : hr;
        char          text[512];
        snprintf(text, sizeof(text), "%s: the GPU device was lost.\n\nReason: %s (0x%08X)", msg,
                 DeviceRemovedReasonName(reason), (unsigned)reason);
        Fatal(text, hr);
    }
    Fatal(msg, hr);
}

void D3D11RenderDevice::CreateBackBufferViews()
{
    ComPtr<ID3D11Texture2D> backBuffer;
    CHECK_HR(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)), "GetBuffer failed");
    CHECK_HR(m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_rtv),
             "CreateRenderTargetView failed");

    D3D11_TEXTURE2D_DESC td = {};
    td.Width      = m_width;
    td.Height     = m_height;
    td.MipLevels  = 1;
    td.ArraySize  = 1;
    td.Format     = kDepthFormat;
    td.SampleDesc = { 1, 0 };
    td.Usage      = D3D11_USAGE_DEFAULT;
    td.BindFlags  = D3D11_BIND_DEPTH_STENCIL;

    ComPtr<ID3D11Texture2D> depth;
    CHECK_HR(m_device->CreateTexture2D(&td, nullptr, &depth), "CreateTexture2D (depth) failed");
    CHECK_HR(m_device->CreateDepthStencilView(depth.Get(), nullptr, &m_dsv), "CreateDepthStencilView failed");
}

std::unique_ptr<IBuffer> D3D11RenderDevice::CreateImmutableBuffer(const void* data, uint32_t sizeBytes, UINT bindFlags)
{
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeBytes;
    bd.Usage     = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = bindFlags;
    D3D11_SUBRESOURCE_DATA init = { data, 0, 0 };

    ComPtr<ID3D11Buffer> buffer;
    CHECK_HR(m_device->CreateBuffer(&bd, &init, &buffer), "CreateBuffer (immutable) failed");
    return std::make_unique<D3D11Buffer>(std::move(buffer), sizeBytes, false);
}

std::unique_ptr<IBuffer> D3D11RenderDevice::CreateVertexBuffer(const void* data, uint32_t sizeBytes)
{
    GAME_ASSERT_CRITICAL(data && sizeBytes > 0, "Invalid vertex data (%p, %u bytes)", data, sizeBytes);
    return CreateImmutableBuffer(data, sizeBytes, D3D11_BIND_VERTEX_BUFFER);
}

std::unique_ptr<IBuffer> D3D11RenderDevice::CreateIndexBuffer(const uint32_t* indices, uint32_t indexCount)
{
    GAME_ASSERT_CRITICAL(indices && indexCount > 0, "Invalid index data (%p, %u indices)", indices, indexCount);
    return CreateImmutableBuffer(indices, indexCount * (uint32_t)sizeof(uint32_t), D3D11_BIND_INDEX_BUFFER);
}

std::unique_ptr<IBuffer> D3D11RenderDevice::CreateDynamicBuffer(uint32_t sizeBytes, UINT bindFlags)
{
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth      = sizeBytes;
    bd.Usage          = D3D11_USAGE_DYNAMIC;
    bd.BindFlags      = bindFlags;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    ComPtr<ID3D11Buffer> buffer;
    CHECK_HR(m_device->CreateBuffer(&bd, nullptr, &buffer), "CreateBuffer (dynamic) failed");
    return std::make_unique<D3D11Buffer>(std::move(buffer), sizeBytes, true);
}

std::unique_ptr<IBuffer> D3D11RenderDevice::CreateDynamicVertexBuffer(uint32_t sizeBytes)
{
    GAME_ASSERT_CRITICAL(sizeBytes > 0, "Empty dynamic vertex buffer");
    return CreateDynamicBuffer(sizeBytes, D3D11_BIND_VERTEX_BUFFER);
}

std::unique_ptr<IBuffer> D3D11RenderDevice::CreateConstantBuffer(uint32_t sizeBytes)
{
    GAME_ASSERT_CRITICAL(sizeBytes > 0 && sizeBytes % 16 == 0, "Constant buffer size %u is not a multiple of 16", sizeBytes);
    return CreateDynamicBuffer(sizeBytes, D3D11_BIND_CONSTANT_BUFFER);
}

std::unique_ptr<IBuffer> D3D11RenderDevice::CreateDynamicStructuredBuffer(uint32_t elementSize, uint32_t elementCount)
{
    GAME_ASSERT_CRITICAL(elementSize > 0 && elementSize % 4 == 0 && elementCount > 0,
                         "Invalid structured buffer (%u x %u bytes)", elementCount, elementSize);

    D3D11_BUFFER_DESC bd   = {};
    bd.ByteWidth           = elementSize * elementCount;
    bd.Usage               = D3D11_USAGE_DYNAMIC;
    bd.BindFlags           = D3D11_BIND_SHADER_RESOURCE;
    bd.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bd.StructureByteStride = elementSize;

    ComPtr<ID3D11Buffer> buffer;
    CHECK_HR(m_device->CreateBuffer(&bd, nullptr, &buffer), "CreateBuffer (structured) failed");

    D3D11_SHADER_RESOURCE_VIEW_DESC sd = {};
    sd.Format              = DXGI_FORMAT_UNKNOWN;
    sd.ViewDimension       = D3D11_SRV_DIMENSION_BUFFER;
    sd.Buffer.FirstElement = 0;
    sd.Buffer.NumElements  = elementCount;

    ComPtr<ID3D11ShaderResourceView> srv;
    CHECK_HR(m_device->CreateShaderResourceView(buffer.Get(), &sd, &srv), "CreateShaderResourceView (structured) failed");
    return std::make_unique<D3D11Buffer>(std::move(buffer), bd.ByteWidth, true, std::move(srv));
}

std::unique_ptr<ITexture> D3D11RenderDevice::CreateTexture(const TextureDesc& desc)
{
    GAME_ASSERT_CRITICAL(desc.data, "Texture without data");
    GAME_ASSERT_CRITICAL(desc.width > 0 && desc.height > 0 &&
                         desc.width <= D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION &&
                         desc.height <= D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION,
                         "Invalid texture size %ux%u", desc.width, desc.height);
    GAME_ASSERT_CRITICAL(desc.mipCount > 0 && desc.mipCount <= D3D11_REQ_MIP_LEVELS &&
                         (std::max(desc.width, desc.height) >> (desc.mipCount - 1)) > 0,
                         "Invalid mip count %u for a %ux%u texture", desc.mipCount, desc.width, desc.height);

    D3D11_TEXTURE2D_DESC td = {};
    td.Width      = desc.width;
    td.Height     = desc.height;
    td.MipLevels  = desc.mipCount;
    td.ArraySize  = 1;
    td.Format     = ToDxgiFormat(desc.format);
    td.SampleDesc = { 1, 0 };
    td.Usage      = D3D11_USAGE_IMMUTABLE;
    td.BindFlags  = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init[D3D11_REQ_MIP_LEVELS] = {};
    const uint32_t         bpp = BytesPerPixel(desc.format);
    const uint8_t*         mip = static_cast<const uint8_t*>(desc.data);
    for (uint32_t i = 0; i < desc.mipCount; ++i)
    {
        const uint32_t w = std::max(desc.width >> i, 1u);
        const uint32_t h = std::max(desc.height >> i, 1u);
        init[i].pSysMem          = mip;
        init[i].SysMemPitch      = w * bpp;
        init[i].SysMemSlicePitch = w * h * bpp;
        mip += (size_t)w * h * bpp;
    }

    ComPtr<ID3D11Texture2D> texture;
    CHECK_HR(m_device->CreateTexture2D(&td, init, &texture), "CreateTexture2D failed");

    ComPtr<ID3D11ShaderResourceView> srv;
    CHECK_HR(m_device->CreateShaderResourceView(texture.Get(), nullptr, &srv), "CreateShaderResourceView failed");
    return std::make_unique<D3D11Texture>(std::move(srv), desc.width, desc.height);
}

void D3D11RenderDevice::UpdateBuffer(IBuffer& buffer, const void* data, uint32_t sizeBytes)
{
    auto& b = static_cast<D3D11Buffer&>(buffer);
    GAME_ASSERT_CRITICAL(b.IsDynamic(), "UpdateBuffer on a buffer that isn't dynamic");
    GAME_ASSERT_CRITICAL(data && sizeBytes > 0 && sizeBytes <= b.GetSizeBytes(),
                         "Updating %u bytes of a %u byte buffer", sizeBytes, b.GetSizeBytes());

    // WRITE_DISCARD hands out fresh memory (buffer renaming), so draws already submitted
    // keep reading the previous contents.
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    CHECK_HR(m_context->Map(b.GetBuffer(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped), "Map (discard) failed");
    memcpy(mapped.pData, data, sizeBytes);
    m_context->Unmap(b.GetBuffer(), 0);
}

std::unique_ptr<IPipeline> D3D11RenderDevice::CreatePipeline(const PipelineDesc& desc)
{
    const ShaderBytecode& vsCode = desc.vertexShader;
    const ShaderBytecode& psCode = desc.pixelShader;
    GAME_ASSERT_CRITICAL(vsCode.data && vsCode.size && psCode.data && psCode.size, "PipelineDesc is missing shader bytecode");
    GAME_ASSERT_CRITICAL(desc.attributes || desc.attributeCount == 0, "Null attributes with count %u", desc.attributeCount);

    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader>  ps;
    CHECK_HR(m_device->CreateVertexShader(vsCode.data, vsCode.size, nullptr, &vs), "CreateVertexShader failed");
    CHECK_HR(m_device->CreatePixelShader(psCode.data, psCode.size, nullptr, &ps), "CreatePixelShader failed");

    D3D11_INPUT_ELEMENT_DESC elements[D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT] = {};
    if (desc.attributeCount > ARRAYSIZE(elements))
        Fatal("Too many vertex attributes");
    for (uint32_t i = 0; i < desc.attributeCount; ++i)
    {
        const VertexAttribute& a  = desc.attributes[i];
        GAME_ASSERT_CRITICAL(a.slot == 0 || (a.slot == 1 && desc.instanceStride > 0),
                             "Attribute %s uses stream %u, which the pipeline doesn't have", a.semantic, a.slot);
        elements[i].SemanticName      = a.semantic;
        elements[i].Format            = ToDxgiFormat(a.format);
        elements[i].InputSlot         = a.slot;
        elements[i].AlignedByteOffset = a.offset;
        if (a.slot == 1 || desc.vertexRate == VertexRate::PerInstance)
        {
            elements[i].InputSlotClass       = D3D11_INPUT_PER_INSTANCE_DATA;
            elements[i].InstanceDataStepRate = 1;
        }
        else
        {
            elements[i].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        }
    }

    ComPtr<ID3D11InputLayout> inputLayout;
    CHECK_HR(m_device->CreateInputLayout(elements, desc.attributeCount,
                                         vsCode.data, vsCode.size, &inputLayout),
             "CreateInputLayout failed");

    D3D11PipelineState state;
    state.topology    = ToD3DTopology(desc.topology);
    state.perInstance    = desc.vertexRate == VertexRate::PerInstance;
    state.instanceStride = desc.instanceStride;

    D3D11_BLEND_DESC                blend = {};
    D3D11_RENDER_TARGET_BLEND_DESC& rt    = blend.RenderTarget[0];
    rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (desc.blend == BlendMode::PremultipliedAlpha)
    {
        rt.BlendEnable    = TRUE;
        rt.SrcBlend       = D3D11_BLEND_ONE;
        rt.DestBlend      = D3D11_BLEND_INV_SRC_ALPHA;
        rt.BlendOp        = D3D11_BLEND_OP_ADD;
        rt.SrcBlendAlpha  = D3D11_BLEND_ONE;
        rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        rt.BlendOpAlpha   = D3D11_BLEND_OP_ADD;
    }
    CHECK_HR(m_device->CreateBlendState(&blend, &state.blend), "CreateBlendState failed");

    D3D11_RASTERIZER_DESC raster = {};
    raster.FillMode        = D3D11_FILL_SOLID;
    raster.CullMode        = desc.cull == CullMode::None ? D3D11_CULL_NONE : D3D11_CULL_BACK;
    raster.DepthClipEnable = TRUE;
    CHECK_HR(m_device->CreateRasterizerState(&raster, &state.rasterizer), "CreateRasterizerState failed");

    D3D11_SAMPLER_DESC sampler = {};
    sampler.Filter         = desc.filter == TextureFilter::Point ? D3D11_FILTER_MIN_MAG_MIP_POINT
                                                                 : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampler.MaxLOD         = D3D11_FLOAT32_MAX;
    CHECK_HR(m_device->CreateSamplerState(&sampler, &state.sampler), "CreateSamplerState failed");

    D3D11_DEPTH_STENCIL_DESC depth = {};
    depth.DepthEnable    = desc.depth != DepthMode::Off;
    depth.DepthWriteMask = desc.depth == DepthMode::ReadWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    depth.DepthFunc      = D3D11_COMPARISON_LESS;
    CHECK_HR(m_device->CreateDepthStencilState(&depth, &state.depthStencil), "CreateDepthStencilState failed");

    return std::make_unique<D3D11Pipeline>(std::move(vs), std::move(ps), std::move(inputLayout),
                                           std::move(state), desc.vertexStride);
}

void D3D11RenderDevice::Resize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0 || (width == m_width && height == m_height))
        return;

    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    m_rtv.Reset();
    m_dsv.Reset();
    CHECK_HR(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, m_swapChainFlags), "ResizeBuffers failed");
    m_width  = width;
    m_height = height;
    CreateBackBufferViews();

    LOG_INFO("Swap chain resized: %ux%u", width, height);
}

void D3D11RenderDevice::BeginFrame(const float clearColor[4])
{
    m_context->ClearRenderTargetView(m_rtv.Get(), clearColor);
    m_context->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    D3D11_VIEWPORT vp = {};
    vp.Width    = (float)m_width;
    vp.Height   = (float)m_height;
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &vp);
    m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), m_dsv.Get());

    // Cheap insurance: whatever ran between frames may have changed the context state.
    InvalidateBoundState();
}

void D3D11RenderDevice::Draw(const DrawDesc& desc)
{
    GAME_ASSERT_CRITICAL(desc.pipeline && desc.vertexBuffer, "Draw without a pipeline or vertex buffer");

    // Only D3D11 resources can come from this device.
    const auto&               p     = static_cast<const D3D11Pipeline&>(*desc.pipeline);
    const auto&               b     = static_cast<const D3D11Buffer&>(*desc.vertexBuffer);
    const auto*               inst  = static_cast<const D3D11Buffer*>(desc.instanceBuffer);
    const auto*               ib    = static_cast<const D3D11Buffer*>(desc.indexBuffer);
    const D3D11PipelineState& state = p.GetState();

    GAME_ASSERT((ib ? desc.indexCount : desc.vertexCount) > 0 && desc.instanceCount > 0,
                "Draw with no vertices/indices or instances");
    GAME_ASSERT((inst != nullptr) == (state.instanceStride > 0),
                "Instance buffer given to a pipeline %s an instance stream", inst ? "without" : "with");

    // Elements read from stream 0: instances for per-instance pipelines, else vertices
    // (only checkable when not indexed).
    const uint64_t lastInstance = (uint64_t)desc.firstInstance + desc.instanceCount;
    if (state.perInstance || !ib)
    {
        const uint64_t lastElement = state.perInstance ? lastInstance : (uint64_t)desc.firstVertex + desc.vertexCount;
        GAME_ASSERT(lastElement * p.GetVertexStride() <= b.GetSizeBytes(),
                    "Drawing %llu elements (stride %u) from a %u byte buffer",
                    (unsigned long long)lastElement, p.GetVertexStride(), b.GetSizeBytes());
    }
    GAME_ASSERT(!inst || lastInstance * state.instanceStride <= inst->GetSizeBytes(),
                "Drawing %llu instances (stride %u) from a %u byte buffer",
                (unsigned long long)lastInstance, state.instanceStride, inst ? inst->GetSizeBytes() : 0);
    GAME_ASSERT(!ib || ((uint64_t)desc.firstIndex + desc.indexCount) * sizeof(uint32_t) <= ib->GetSizeBytes(),
                "Drawing %u indices from index %u of a %u byte index buffer",
                desc.indexCount, desc.firstIndex, ib ? ib->GetSizeBytes() : 0);

    // Only set what changed since the last draw (consecutive draws usually share most state).
    BoundState&  bound = m_bound;
    const bool   all   = !bound.valid;
    bound.valid        = true;
    auto changed = [all](auto& current, auto wanted) {
        if (!all && current == wanted)
            return false;
        current = wanted;
        return true;
    };

    if (changed(bound.inputLayout, p.GetInputLayout()))
        m_context->IASetInputLayout(bound.inputLayout);
    if (changed(bound.topology, state.topology))
        m_context->IASetPrimitiveTopology(bound.topology);

    ID3D11Buffer* vbs[2]     = { b.GetBuffer(), inst ? inst->GetBuffer() : nullptr };
    const UINT    strides[2] = { p.GetVertexStride(), state.instanceStride };
    const UINT    offsets[2] = { 0, 0 };
    if (all || vbs[0] != bound.vertexBuffers[0] || vbs[1] != bound.vertexBuffers[1] ||
        strides[0] != bound.strides[0] || strides[1] != bound.strides[1])
    {
        m_context->IASetVertexBuffers(0, 2, vbs, strides, offsets);
        for (int i = 0; i < 2; ++i)
        {
            bound.vertexBuffers[i] = vbs[i];
            bound.strides[i]       = strides[i];
        }
    }
    if (changed(bound.indexBuffer, ib ? ib->GetBuffer() : nullptr))
        m_context->IASetIndexBuffer(bound.indexBuffer, DXGI_FORMAT_R32_UINT, 0);

    if (changed(bound.vs, p.GetVertexShader()))
        m_context->VSSetShader(bound.vs, nullptr, 0);
    if (changed(bound.ps, p.GetPixelShader()))
        m_context->PSSetShader(bound.ps, nullptr, 0);
    if (changed(bound.rasterizer, state.rasterizer.Get()))
        m_context->RSSetState(bound.rasterizer);
    if (changed(bound.blend, state.blend.Get()))
        m_context->OMSetBlendState(bound.blend, nullptr, 0xFFFFFFFF);
    if (changed(bound.depthStencil, state.depthStencil.Get()))
        m_context->OMSetDepthStencilState(bound.depthStencil, 0);

    ID3D11Buffer* cb = desc.constantBuffer ? static_cast<const D3D11Buffer*>(desc.constantBuffer)->GetBuffer() : nullptr;
    if (changed(bound.constantBuffer, cb))
    {
        m_context->VSSetConstantBuffers(0, 1, &cb);
        m_context->PSSetConstantBuffers(0, 1, &cb);
    }

    ID3D11ShaderResourceView* srv = desc.texture ? static_cast<const D3D11Texture*>(desc.texture)->GetSrv() : nullptr;
    if (changed(bound.psSrv, srv))
        m_context->PSSetShaderResources(0, 1, &srv);
    ID3D11SamplerState* sampler = state.sampler.Get();
    if (changed(bound.sampler, sampler))
        m_context->PSSetSamplers(0, 1, &sampler);

    const auto*               vsBuffer = static_cast<const D3D11Buffer*>(desc.vertexShaderBuffer);
    ID3D11ShaderResourceView* vsSrv    = vsBuffer ? vsBuffer->GetSrv() : nullptr;
    GAME_ASSERT(!vsBuffer || vsSrv, "vertexShaderBuffer isn't a structured buffer");
    if (changed(bound.vsSrv, vsSrv))
        m_context->VSSetShaderResources(0, 1, &vsSrv);

    if (ib)
        m_context->DrawIndexedInstanced(desc.indexCount, desc.instanceCount, desc.firstIndex, 0, desc.firstInstance);
    else
        m_context->DrawInstanced(desc.vertexCount, desc.instanceCount, desc.firstVertex, desc.firstInstance);
}

void D3D11RenderDevice::EndFrame()
{
    // Device loss (driver update/crash, GPU timeout) shows up here first; report it by name
    // rather than letting an unrelated call fail later. Recovering would need every resource
    // recreated, which the engine doesn't support.
    const HRESULT hr = m_swapChain->Present(m_vsync ? 1 : 0, m_vsync ? 0 : m_presentFlags);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        FailHr("Present failed", hr);
    else if (FAILED(hr))
        LOG_WARN("Present failed (0x%08X)", (unsigned)hr);
}

#if defined(GAME_IMGUI_ENABLED)
void D3D11RenderDevice::ImGuiInitBackend()
{
    ImGui_ImplDX11_Init(m_device.Get(), m_context.Get());
}

void D3D11RenderDevice::ImGuiShutdownBackend()
{
    ImGui_ImplDX11_Shutdown();
}

void D3D11RenderDevice::ImGuiNewFrame()
{
    ImGui_ImplDX11_NewFrame();
}

void D3D11RenderDevice::ImGuiRender()
{
    // The backend saves and restores the D3D11 state it touches.
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    InvalidateBoundState(); // restored, but not necessarily to what we had bound
}
#endif

#if defined(GAME_DEV_TOOLS_ENABLED)
bool D3D11RenderDevice::CaptureBackBuffer(std::vector<uint8_t>& rgba, uint32_t& width, uint32_t& height)
{
    ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
        return false;

    D3D11_TEXTURE2D_DESC desc = {};
    backBuffer->GetDesc(&desc);
    desc.Usage          = D3D11_USAGE_STAGING;
    desc.BindFlags      = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags      = 0;

    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(m_device->CreateTexture2D(&desc, nullptr, &staging)))
        return false;
    m_context->CopyResource(staging.Get(), backBuffer.Get());

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(m_context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
        return false;

    width  = desc.Width;
    height = desc.Height;
    rgba.resize((size_t)width * height * 4);
    for (uint32_t y = 0; y < height; ++y)
    {
        const uint8_t* src = static_cast<const uint8_t*>(mapped.pData) + (size_t)y * mapped.RowPitch;
        uint8_t*       dst = rgba.data() + (size_t)y * width * 4;
        memcpy(dst, src, (size_t)width * 4);
        for (uint32_t x = 0; x < width; ++x)
            dst[x * 4 + 3] = 255;
    }
    m_context->Unmap(staging.Get(), 0);
    return true;
}
#endif

std::unique_ptr<IRenderDevice> CreateRenderDevice(IWindow& window, const RenderDeviceDesc& desc)
{
    return std::make_unique<D3D11RenderDevice>(static_cast<HWND>(window.GetNativeHandle()),
                                               window.GetWidth(), window.GetHeight(), desc);
}
