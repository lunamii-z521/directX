#pragma once

constexpr uint32_t FRAME_COUNT = 2;

class D3D12Context
{
public:
    D3D12Context() = default;
    D3D12Context(HWND hwnd, uint32_t width, uint32_t height);
    virtual ~D3D12Context();

    bool Init(HWND hwnd, uint32_t width, uint32_t height);
    void Shutdown();
    void BeginFrame();
    void EndFrame();
    void WaitForGPU();

    ID3D12Device* GetDevice() const { return m_device.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return m_commandList.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTVHandle() const;
    ID3D12Resource* GetCurrentRenderTarget() const { return m_renderTargets[m_frameIndex].Get(); }
    UINT GetFrameIndex() const { return m_frameIndex; }

private:
    #pragma region core context objects

    // main objects
    ComPtr<ID3D12Device>                m_device;
    ComPtr<IDXGIFactory4>               m_factory;

    // rendering cmd
    ComPtr<ID3D12GraphicsCommandList>   m_commandList;
    ComPtr<ID3D12CommandQueue>          m_commandQueue;
    ComPtr<ID3D12CommandAllocator>      m_commandAllocator;

    // swap chain
    ComPtr<IDXGISwapChain3>             m_swapChain;

    // render target view(rtv) objects
    ComPtr<ID3D12DescriptorHeap>        m_rtvHeap;
    UINT                                m_rtvDescriptorSize = 0;

    // close-to-screen rendering objects
    UINT                                m_frameIndex = 0;
    ComPtr<ID3D12Resource>              m_renderTargets[FRAME_COUNT];

    // fence & sync
    ComPtr<ID3D12Fence>                 m_fence;
    UINT64                              m_fenceValue;
    HANDLE                              m_fenceEvent = nullptr;

    // window props
    HWND m_hwnd = nullptr;
    uint32_t m_width = 1280;
    uint32_t m_height = 720;

    #pragma endregion
};

