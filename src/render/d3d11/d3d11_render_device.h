#pragma once

#include "render/render_device.h"

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_5.h>
#include <wrl/client.h>

class D3D11Buffer final : public IBuffer
{
public:
    D3D11Buffer(Microsoft::WRL::ComPtr<ID3D11Buffer> buffer, uint32_t sizeBytes, bool dynamic,
                Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv = nullptr);

    uint32_t      GetSizeBytes() const override { return m_sizeBytes; }
    ID3D11Buffer* GetBuffer() const { return m_buffer.Get(); }
    bool          IsDynamic() const { return m_dynamic; }
    // Only structured buffers have one.
    ID3D11ShaderResourceView* GetSrv() const { return m_srv.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer>             m_buffer;
    uint32_t                                         m_sizeBytes;
    bool                                             m_dynamic;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_srv;
};

class D3D11Texture final : public ITexture
{
public:
    D3D11Texture(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv, uint32_t width, uint32_t height);

    uint32_t GetWidth() const override { return m_width; }
    uint32_t GetHeight() const override { return m_height; }

    ID3D11ShaderResourceView* GetSrv() const { return m_srv.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_srv;
    uint32_t                                         m_width;
    uint32_t                                         m_height;
};

// Fixed-function state baked into a pipeline.
struct D3D11PipelineState
{
    Microsoft::WRL::ComPtr<ID3D11BlendState>      blend;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>      sampler;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencil;
    D3D11_PRIMITIVE_TOPOLOGY                        topology       = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    bool                                            perInstance    = false; // stream 0 is stepped per instance
    uint32_t                                        instanceStride = 0;     // stream 1 (0 = unused)
};

class D3D11Pipeline final : public IPipeline
{
public:
    D3D11Pipeline(Microsoft::WRL::ComPtr<ID3D11VertexShader> vs,
                  Microsoft::WRL::ComPtr<ID3D11PixelShader>  ps,
                  Microsoft::WRL::ComPtr<ID3D11InputLayout>  inputLayout,
                  D3D11PipelineState                         state,
                  uint32_t                                   vertexStride);

    uint32_t GetVertexStride() const override { return m_vertexStride; }

    ID3D11VertexShader* GetVertexShader() const { return m_vs.Get(); }
    ID3D11PixelShader*  GetPixelShader() const { return m_ps.Get(); }
    ID3D11InputLayout*  GetInputLayout() const { return m_inputLayout.Get(); }
    const D3D11PipelineState& GetState() const { return m_state; }

private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_ps;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_inputLayout;
    D3D11PipelineState                         m_state;
    uint32_t                                   m_vertexStride;
};

class D3D11RenderDevice final : public IRenderDevice
{
public:
    D3D11RenderDevice(HWND hwnd, uint32_t width, uint32_t height, const RenderDeviceDesc& desc);
    ~D3D11RenderDevice() override;

    std::unique_ptr<IBuffer>   CreateVertexBuffer(const void* data, uint32_t sizeBytes) override;
    std::unique_ptr<IBuffer>   CreateIndexBuffer(const uint32_t* indices, uint32_t indexCount) override;
    std::unique_ptr<IBuffer>   CreateDynamicVertexBuffer(uint32_t sizeBytes) override;
    std::unique_ptr<IBuffer>   CreateConstantBuffer(uint32_t sizeBytes) override;
    std::unique_ptr<IBuffer>   CreateDynamicStructuredBuffer(uint32_t elementSize, uint32_t elementCount) override;
    std::unique_ptr<ITexture>  CreateTexture(const TextureDesc& desc) override;
    std::unique_ptr<IPipeline> CreatePipeline(const PipelineDesc& desc) override;

    void UpdateBuffer(IBuffer& buffer, const void* data, uint32_t sizeBytes) override;

    void Resize(uint32_t width, uint32_t height) override;

    void BeginFrame(const float clearColor[4]) override;
    using IRenderDevice::Draw;
    void Draw(const DrawDesc& desc) override;
    void EndFrame() override;

#if defined(GAME_IMGUI_ENABLED)
    void ImGuiInitBackend() override;
    void ImGuiShutdownBackend() override;
    void ImGuiNewFrame() override;
    void ImGuiRender() override;
#endif

#if defined(GAME_DEV_TOOLS_ENABLED)
    bool CaptureBackBuffer(std::vector<uint8_t>& rgba, uint32_t& width, uint32_t& height) override;
#endif

private:
    // Fatal error for a failed call; a removed/reset device is reported with its reason.
    [[noreturn]] void FailHr(const char* msg, HRESULT hr) const;
    // Render target view and depth buffer for the current swap chain size.
    void CreateBackBufferViews();
    // Forgets what Draw last bound (after someone else changed the context state).
    void InvalidateBoundState() { m_bound = {}; }
    std::unique_ptr<IBuffer> CreateImmutableBuffer(const void* data, uint32_t sizeBytes, UINT bindFlags);
    std::unique_ptr<IBuffer> CreateDynamicBuffer(uint32_t sizeBytes, UINT bindFlags);

    Microsoft::WRL::ComPtr<ID3D11Device>           m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>    m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain1>        m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_dsv;
    uint32_t                                       m_width  = 0;
    uint32_t                                       m_height = 0;
    bool                                           m_vsync  = true;
    UINT                                           m_swapChainFlags = 0;
    UINT                                           m_presentFlags   = 0;
    bool                                           m_debugLayer     = false;

    // What Draw bound last, so unchanged state isn't set again. Only D3D objects are compared:
    // the context references whatever is bound, so a bound pointer can't be freed and reused
    // by a new object (our own IPipeline/IBuffer wrappers could be).
    struct BoundState
    {
        bool                      valid            = false; // false: rebind everything
        ID3D11InputLayout*        inputLayout      = nullptr;
        D3D11_PRIMITIVE_TOPOLOGY  topology         = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
        ID3D11VertexShader*       vs               = nullptr;
        ID3D11PixelShader*        ps               = nullptr;
        ID3D11RasterizerState*    rasterizer       = nullptr;
        ID3D11BlendState*         blend            = nullptr;
        ID3D11DepthStencilState*  depthStencil     = nullptr;
        ID3D11SamplerState*       sampler          = nullptr;
        ID3D11Buffer*             vertexBuffers[2] = {};
        UINT                      strides[2]       = {};
        ID3D11Buffer*             indexBuffer      = nullptr;
        ID3D11Buffer*             constantBuffer   = nullptr;
        ID3D11ShaderResourceView* psSrv            = nullptr;
        ID3D11ShaderResourceView* vsSrv            = nullptr;
    };
    BoundState m_bound;
};
