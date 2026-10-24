#include "pch.h"
#include "D3D12Context.h"

D3D12Context::D3D12Context(HWND hwnd, uint32_t width, uint32_t height) { Init(hwnd, width, height); }
D3D12Context::~D3D12Context() { Shutdown(); }

bool D3D12Context::Init(HWND hwnd, uint32_t width, uint32_t height)
{
    // creating device & factory
    D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
    CreateDXGIFactory2(0, IID_PPV_ARGS(&m_factory));

    // creating cmd queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));

    // creating swap chain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FRAME_COUNT;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;
    ComPtr<IDXGISwapChain1> swapChain1;
    m_factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1
    );
    swapChain1.As(&m_swapChain);
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // creating rtvs
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FRAME_COUNT;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < FRAME_COUNT; i++)
    {
        m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += m_rtvDescriptorSize; // Offset handle pointer manually
    }

    // creating cmd
    m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator));
    m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList));
    m_commandList->Close();
    m_commandAllocator->Reset();
    m_commandList->Reset(m_commandAllocator.Get(), nullptr);

    // creating fence
    m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    return true;
}
void D3D12Context::Shutdown()
{
    WaitForGPU();
    CloseHandle(m_fenceEvent);
}

void D3D12Context::BeginFrame()
{
    m_commandAllocator->Reset();
    m_commandList->Reset(m_commandAllocator.Get(), nullptr);
}

void D3D12Context::EndFrame()
{
    m_commandList->Close();
    ID3D12CommandList* ppCommandLists[] = {m_commandList.Get()};
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    m_swapChain->Present(1, 0);
    WaitForGPU();

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void D3D12Context::WaitForGPU()
{
    m_fenceValue++;
    m_commandQueue->Signal(m_fence.Get(), m_fenceValue);

    if (m_fence->GetCompletedValue() < m_fenceValue)
    {
        m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Context::GetCurrentRTVHandle() const
{
    D3D12_CPU_DESCRIPTOR_HANDLE res(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    res.ptr += m_frameIndex * m_rtvDescriptorSize;
    return res;
}
