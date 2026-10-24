#include "pch.h"

#include "core/D3D12Context.h"
#include "core/D3D12VertexBuffer.h"
#include "core/D3D12IndexBuffer.h"

constexpr uint32_t WIDTH = 1280;
constexpr uint32_t HEIGHT = 720;
D3D12Context context;

struct Vertex
{
    XMFLOAT3 position; // x, y, z
    XMFLOAT4 color;    // r, g, b, a
};
static constexpr UINT CalculateConstantBufferSize(UINT byteSize)
{
    return (byteSize + 255) & ~255;
}
struct ObjectConstants
{
    XMMATRIX model = XMMatrixIdentity();
    XMMATRIX vp = XMMatrixIdentity();
};

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
    context.Init(hwnd, WIDTH, HEIGHT);

    float quadVertices[] = {
    -0.5f,  0.5f, 2.0f, 1.0f, 0.0f, 0.0f, 1.0f, // 0: Top-Left
    0.5f,  0.5f, 2.0f,  0.0f, 1.0f, 0.0f, 1.0f, // 1: Top-Right
    0.5f, -0.5f, 2.0f,  0.0f, 0.0f, 1.0f, 1.0f, // 2: Bottom-Right
    -0.5f, -0.5f, 2.0f, 1.0f, 1.0f, 0.0f, 1.0f  // 3: Bottom-Left
    };
    uint32_t quadIndices[] = {
    0, 1, 2, // Triangle 1: Top-Left -> Top-Right -> Bottom-Right
    0, 2, 3  // Triangle 2: Top-Left -> Bottom-Right -> Bottom-Left
    };
    D3D12VertexBuffer vb(quadVertices, sizeof(quadVertices), {{"POSITION", DXGI_FORMAT_R32G32B32_FLOAT}, {"COLOR", DXGI_FORMAT_R32G32B32A32_FLOAT}});
    D3D12IndexBuffer ib(quadIndices, sizeof(quadIndices));
    
    #pragma region root signature
    D3D12_ROOT_PARAMETER rootParams = {};
    rootParams.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams.Descriptor.ShaderRegister = 0;
    rootParams.Descriptor.RegisterSpace = 0;
    rootParams.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    ComPtr<ID3D12RootSignature> rootSignature;
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = 1;
    rootSignatureDesc.pParameters = &rootParams;
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
    hr = context.GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature)
    );
    if (FAILED(hr))
    {
        std::cout << "Failed to create Root Signature! HR: " << std::hex << hr << std::endl;
        return -1;
    }
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
    #pragma region cb
    ComPtr<ID3D12Resource> constantBuffer;
    UINT8* pMappedConstantBuffer = nullptr;
    D3D12_HEAP_PROPERTIES cbHeapProps = {};
    cbHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC cbDesc = {};
    cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    cbDesc.Width = CalculateConstantBufferSize(sizeof(ObjectConstants));
    cbDesc.Height = 1;
    cbDesc.DepthOrArraySize = 1;
    cbDesc.MipLevels = 1;
    cbDesc.Format = DXGI_FORMAT_UNKNOWN;
    cbDesc.SampleDesc.Count = 1;
    cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    context.GetDevice()->CreateCommittedResource(
        &cbHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &cbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&constantBuffer)
    );
    D3D12_RANGE readRange = {0, 0}; // We won't read from CPU
    constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pMappedConstantBuffer));
    #pragma endregion
    #pragma region pso
    ComPtr<ID3D12PipelineState> pipelineState;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = {vertexShader->GetBufferPointer(), vertexShader->GetBufferSize()};
    psoDesc.PS = {pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
    psoDesc.InputLayout = vb.GetInputDesc();
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
    hr = context.GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
    if (FAILED(hr))
    {
        std::cout << "Failed to create Graphics Pipeline State! HR: " << std::hex << hr << std::endl;
        return -1;
    }
    #pragma endregion
    
    context.EndFrame();

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        context.BeginFrame();

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = context.GetCurrentRenderTarget();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        
        context.GetCommandList()->ResourceBarrier(1, &barrier);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = context.GetCurrentRTVHandle();

        constexpr float clearColor[] = {0.06f, 0.06f, 0.06f, 1.0f};
        context.GetCommandList()->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

        D3D12_VIEWPORT viewport = {0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f};
        D3D12_RECT scissorRect = {0, 0, static_cast<LONG>(WIDTH), static_cast<LONG>(HEIGHT)};
        float aspectRatio = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
        XMMATRIX model = XMMatrixRotationZ(static_cast<float>(glfwGetTime())); // Rotate quad over time
        XMMATRIX view = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
        XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), aspectRatio, 0.1f, 100.0f);
        ObjectConstants cbData;
        cbData.model = XMMatrixTranspose(model);
        cbData.vp = XMMatrixTranspose(view * proj);
        memcpy(pMappedConstantBuffer, &cbData, sizeof(ObjectConstants));

        context.GetCommandList()->RSSetViewports(1, &viewport);
        context.GetCommandList()->RSSetScissorRects(1, &scissorRect);

        context.GetCommandList()->SetGraphicsRootSignature(rootSignature.Get());
        context.GetCommandList()->SetPipelineState(pipelineState.Get());

        D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = constantBuffer->GetGPUVirtualAddress();
        context.GetCommandList()->SetGraphicsRootConstantBufferView(0, cbvAddress);

        context.GetCommandList()->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        context.GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        D3D12_VERTEX_BUFFER_VIEW vbView = vb.GetVBView();
        context.GetCommandList()->IASetVertexBuffers(0, 1, &vbView);
        D3D12_INDEX_BUFFER_VIEW ibView = ib.GetIBView();
        context.GetCommandList()->IASetIndexBuffer(&ibView);

        context.GetCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        context.GetCommandList()->ResourceBarrier(1, &barrier);

        context.EndFrame();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}