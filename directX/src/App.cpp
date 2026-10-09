#include <iostream>

// Request Win32 native handle support from GLFW
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h> // Microsoft::WRL::ComPtr

using namespace DirectX;
using Microsoft::WRL::ComPtr;

constexpr uint32_t WIDTH = 1280;
constexpr uint32_t HEIGHT = 720;
constexpr uint32_t FRAME_COUNT = 2;

// DX12 Core Objects
ComPtr<ID3D12Device>           g_device;
ComPtr<ID3D12CommandQueue>     g_commandQueue;
ComPtr<IDXGISwapChain3>        g_swapChain;
ComPtr<ID3D12DescriptorHeap>   g_rtvHeap;
ComPtr<ID3D12Resource>         g_renderTargets[FRAME_COUNT];
ComPtr<ID3D12CommandAllocator> g_commandAllocator;
ComPtr<ID3D12GraphicsCommandList> g_commandList;

// Synchronization
ComPtr<ID3D12Fence>            g_fence;
UINT64                         g_fenceValue = 0;
HANDLE                         g_fenceEvent = nullptr;
UINT                           g_frameIndex = 0;
UINT                           g_rtvDescriptorSize = 0;

struct Vertex
{
    XMFLOAT3 position; // x, y, z
    XMFLOAT4 color;    // r, g, b, a
};

static void WaitForGpu()
{
    g_fenceValue++;
    g_commandQueue->Signal(g_fence.Get(), g_fenceValue);

    if (g_fence->GetCompletedValue() < g_fenceValue)
    {
        g_fence->SetEventOnCompletion(g_fenceValue, g_fenceEvent);
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }
}

int main()
{
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "DirectX 12 Render Project", nullptr, nullptr);
    HWND hwnd = glfwGetWin32Window(window);

    #ifdef _DEBUG
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
    }
    #endif

    ComPtr<IDXGIFactory4> factory;
    CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_device));

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    g_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&g_commandQueue));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FRAME_COUNT;
    swapChainDesc.Width = WIDTH;
    swapChainDesc.Height = HEIGHT;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;
    ComPtr<IDXGISwapChain1> swapChain1;
    factory->CreateSwapChainForHwnd(
        g_commandQueue.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1
    );
    swapChain1.As(&g_swapChain);
    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FRAME_COUNT;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    g_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_rtvHeap));

    g_rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(g_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < FRAME_COUNT; i++)
    {
        g_swapChain->GetBuffer(i, IID_PPV_ARGS(&g_renderTargets[i]));
        g_device->CreateRenderTargetView(g_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += g_rtvDescriptorSize; // Offset handle pointer manually
    }

    g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_commandAllocator));
    g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&g_commandList));
    g_commandList->Close(); // Command lists are created in recording state; close it initially

    g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence));
    g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    #pragma region vertex buffer
    Vertex quadVertices[] = {
    { XMFLOAT3(-0.5f,  0.5f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) }, // 0: Top-Left (Red)
    { XMFLOAT3(0.5f,  0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) }, // 1: Top-Right (Green)
    { XMFLOAT3(0.5f, -0.5f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) }, // 2: Bottom-Right (Blue)
    { XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) }  // 3: Bottom-Left (Yellow)
    };
    ComPtr<ID3D12Resource> vertexBuffer;
    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = sizeof(quadVertices);
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    g_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertexBuffer)
    );
    UINT8* pVertexDataBegin = nullptr;
    D3D12_RANGE vbreadRange = {0, 0}; // CPU won't read this memory back
    vertexBuffer->Map(0, &vbreadRange, reinterpret_cast<void**>(&pVertexDataBegin));
    memcpy(pVertexDataBegin, quadVertices, sizeof(quadVertices));
    vertexBuffer->Unmap(0, nullptr);
    D3D12_VERTEX_BUFFER_VIEW vbView = {};
    vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress(); // Raw VRAM address
    vbView.StrideInBytes = sizeof(Vertex);
    vbView.SizeInBytes = sizeof(quadVertices);                // Total size
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,                            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {inputElementDescs, _countof(inputElementDescs)};
    #pragma endregion
    #pragma region index buffer
    uint16_t quadIndices[] = {
    0, 1, 2, // Triangle 1: Top-Left -> Top-Right -> Bottom-Right
    0, 2, 3  // Triangle 2: Top-Left -> Bottom-Right -> Bottom-Left
    };
    ComPtr<ID3D12Resource> indexBuffer;
    D3D12_RESOURCE_DESC ibDesc = {};
    ibDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    ibDesc.Width = sizeof(quadIndices);
    ibDesc.Height = 1;
    ibDesc.DepthOrArraySize = 1;
    ibDesc.MipLevels = 1;
    ibDesc.Format = DXGI_FORMAT_UNKNOWN;
    ibDesc.SampleDesc.Count = 1;
    ibDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    g_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &ibDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&indexBuffer)
    );
    UINT8* pIndexDataBegin = nullptr;
    D3D12_RANGE ibreadRange = {0, 0};
    indexBuffer->Map(0, &ibreadRange, reinterpret_cast<void**>(&pIndexDataBegin));
    memcpy(pIndexDataBegin, quadIndices, sizeof(quadIndices));
    indexBuffer->Unmap(0, nullptr);
    D3D12_INDEX_BUFFER_VIEW ibView = {};
    ibView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
    ibView.SizeInBytes = sizeof(quadIndices);
    ibView.Format = DXGI_FORMAT_R16_UINT;
    #pragma endregion
    #pragma region root signature
    ComPtr<ID3D12RootSignature> rootSignature;
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = 0;
    rootSignatureDesc.pParameters = nullptr;
    rootSignatureDesc.NumStaticSamplers = 0;
    rootSignatureDesc.pStaticSamplers = nullptr;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            std::cout << "Root Signature Error:\n"
                << static_cast<char*>(errorBlob->GetBufferPointer()) << std::endl;
        }
        return -1;
    }
    hr = g_device->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature)
    );
    if (FAILED(hr)) std::cout << "Failed to create PSO! HR: " << std::hex << hr << std::endl;
    #pragma endregion
    #pragma region shaders
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
    UINT compileFlags = 0;
    #ifdef _DEBUG
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    #endif
    hr = D3DCompileFromFile(
        L"assets/shader.hlsl",
        nullptr,
        nullptr,
        "VSMain",
        "vs_5_0",
        compileFlags,
        0,
        &vertexShader,
        &errorBlob
    );
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            std::cout << "Vertex Shader Error:\n" << static_cast<char*>(errorBlob->GetBufferPointer()) << std::endl;
        }
        return -1;
    }
    hr = D3DCompileFromFile(
        L"assets/shader.hlsl",
        nullptr,
        nullptr,
        "PSMain",
        "ps_5_0",
        compileFlags,
        0,
        &pixelShader,
        &errorBlob
    );
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            std::cout << "Pixel Shader Error:\n" << static_cast<char*>(errorBlob->GetBufferPointer()) << std::endl;
        }
        return -1;
    }
    #pragma endregion
    #pragma region pso
    ComPtr<ID3D12PipelineState> pipelineState;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = {vertexShader->GetBufferPointer(), vertexShader->GetBufferSize()};
    psoDesc.PS = {pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
    psoDesc.InputLayout = inputLayoutDesc;
    // rasterizing
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.RasterizerState.MultisampleEnable = FALSE;
    psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
    psoDesc.RasterizerState.ForcedSampleCount = 0;
    psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    // blending
    psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
    psoDesc.BlendState.IndependentBlendEnable = FALSE;
    const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
        FALSE, FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL,
    };
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        psoDesc.BlendState.RenderTarget[i] = defaultRenderTargetBlendDesc;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    hr = g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
    if (FAILED(hr))
    {
        std::cout << "Failed to create Graphics Pipeline State! HR: " << std::hex << hr << std::endl;
        return -1;
    }
    #pragma endregion

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        g_commandAllocator->Reset();
        g_commandList->Reset(g_commandAllocator.Get(), nullptr);

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = g_renderTargets[g_frameIndex].Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        g_commandList->ResourceBarrier(1, &barrier);

        D3D12_CPU_DESCRIPTOR_HANDLE currentRtvHandle(g_rtvHeap->GetCPUDescriptorHandleForHeapStart());
        currentRtvHandle.ptr += static_cast<unsigned long long>(g_frameIndex) * g_rtvDescriptorSize;

        constexpr float clearColor[] = {0.06f, 0.06f, 0.06f, 1.0f};
        g_commandList->ClearRenderTargetView(currentRtvHandle, clearColor, 0, nullptr);

        D3D12_VIEWPORT viewport = {0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f};
        D3D12_RECT scissorRect = {0, 0, static_cast<LONG>(WIDTH), static_cast<LONG>(HEIGHT)};

        g_commandList->RSSetViewports(1, &viewport);
        g_commandList->RSSetScissorRects(1, &scissorRect);

        g_commandList->SetGraphicsRootSignature(rootSignature.Get());
        g_commandList->SetPipelineState(pipelineState.Get());

        g_commandList->OMSetRenderTargets(1, &currentRtvHandle, FALSE, nullptr);

        g_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        g_commandList->IASetVertexBuffers(0, 1, &vbView);
        g_commandList->IASetIndexBuffer(&ibView);

        g_commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        g_commandList->ResourceBarrier(1, &barrier);

        g_commandList->Close();
        ID3D12CommandList* ppCommandLists[] = {g_commandList.Get()};
        g_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

        g_swapChain->Present(1, 0);
        WaitForGpu(); // Simple sync for learning: ensure GPU finishes before next frame

        g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();
    }

    // Cleanup GPU execution before exit
    WaitForGpu();
    CloseHandle(g_fenceEvent);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}