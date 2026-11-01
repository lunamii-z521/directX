#include "pch.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"

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
    context.BeginFrame();

    float quadVertices[] = {
    -0.5f,  0.5f, 2.0f,  1.0f, 1.0f, 1.0f, 1.0f,  0.f, 1.f, // 0: Top-Left
     0.5f,  0.5f, 2.0f,  1.0f, 1.0f, 1.0f, 1.0f,  1.f, 1.f, // 1: Top-Right
     0.5f, -0.5f, 2.0f,  1.0f, 1.0f, 1.0f, 1.0f,  1.f, 0.f, // 2: Bottom-Right
    -0.5f, -0.5f, 2.0f,  1.0f, 1.0f, 1.0f, 1.0f,  0.f, 0.f, // 3: Bottom-Left
    };
    uint32_t quadIndices[] = {
    0, 1, 2, // Triangle 1: Top-Left -> Top-Right -> Bottom-Right
    0, 2, 3  // Triangle 2: Top-Left -> Bottom-Right -> Bottom-Left
    };
    D3D12VertexBuffer vb(quadVertices, sizeof(quadVertices),
                        {{"POSITION", DXGI_FORMAT_R32G32B32_FLOAT}, {"COLOR", DXGI_FORMAT_R32G32B32A32_FLOAT}, {"TEXCOORD", DXGI_FORMAT_R32G32_FLOAT}});
    D3D12IndexBuffer ib(quadIndices, sizeof(quadIndices));
    
    #pragma region texture
    int width, height, channels;
    unsigned char* imageData = stbi_load("assets/brick_diffuse.png", &width, &height, &channels, 4);
    if (!imageData) { std::cout << "Failed to load texture image!\n"; return -1; }
    UINT64 imageSize = static_cast<UINT64>(width) * height * 4;

    ComPtr<ID3D12Resource> textureResource;
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Alignment = 0;
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES defaultHeapProps = {};
    defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    context.GetDevice()->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&textureResource)
    );

    ComPtr<ID3D12Resource> uploadHeap;

    UINT64 uploadBufferSize = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT ayakLayout;
    UINT numRows = 0;
    UINT64 rowSizeInBytes = 0;

    context.GetDevice()->GetCopyableFootprints(&textureDesc, 0, 1, 0, &ayakLayout, &numRows, &rowSizeInBytes, &uploadBufferSize);

    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadBufferDesc = {};
    uploadBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadBufferDesc.Width = uploadBufferSize;
    uploadBufferDesc.Height = 1;
    uploadBufferDesc.DepthOrArraySize = 1;
    uploadBufferDesc.MipLevels = 1;
    uploadBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadBufferDesc.SampleDesc.Count = 1;
    uploadBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    context.GetDevice()->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadHeap)
    );

    // Map upload heap and copy row by row to respect row pitch padding
    UINT8* mappedUpload = nullptr;
    uploadHeap->Map(0, nullptr, reinterpret_cast<void**>(&mappedUpload));

    for (UINT y = 0; y < numRows; ++y)
    {
        memcpy(
            mappedUpload + ayakLayout.Offset + y * ayakLayout.Footprint.RowPitch,
            imageData + y * width * 4,
            width * 4
        );
    }
    uploadHeap->Unmap(0, nullptr);
    stbi_image_free(imageData); // Free CPU image memory

    // Record copy command from upload heap -> default VRAM heap
    D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
    dstLocation.pResource = textureResource.Get();
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLocation.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
    srcLocation.pResource = uploadHeap.Get();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLocation.PlacedFootprint = ayakLayout;

    context.GetCommandList()->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

    // Transition resource state: COPY_DEST -> PIXEL_SHADER_RESOURCE (ready for sampling)
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = textureResource.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    context.GetCommandList()->ResourceBarrier(1, &barrier);
    #pragma endregion

    ComPtr<ID3D12Resource> depthStencilBuffer;

    D3D12_RESOURCE_DESC depthDesc = {};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Alignment = 0;
    depthDesc.Width = WIDTH;
    depthDesc.Height = HEIGHT;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT; // 32-bit float depth
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; 

    D3D12_HEAP_PROPERTIES depthHeapProps = {};
    depthHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
    depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthOptimizedClearValue.DepthStencil.Depth = 1.0f; // Clear to farthest depth (1.0)
    depthOptimizedClearValue.DepthStencil.Stencil = 0;

    context.GetDevice()->CreateCommittedResource(
        &depthHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, // Initial state
        &depthOptimizedClearValue,
        IID_PPV_ARGS(&depthStencilBuffer)
    );

    ComPtr<ID3D12DescriptorHeap> dsvHeap;

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    context.GetDevice()->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));

    // Create the Depth-Stencil View
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvViewDesc = {};
    dsvViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvViewDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvViewDesc.Texture2D.MipSlice = 0;

    context.GetDevice()->CreateDepthStencilView(
        depthStencilBuffer.Get(),
        &dsvViewDesc,
        dsvHeap->GetCPUDescriptorHandleForHeapStart()
    );


    // 1. Create Descriptor Heap
    ComPtr<ID3D12DescriptorHeap> srvHeap;
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 1; // Room for 1 texture SRV
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // Must be shader-visible!
    context.GetDevice()->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap));

    // 2. Create the SRV inside the heap
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    context.GetDevice()->CreateShaderResourceView(
        textureResource.Get(),
        &srvDesc,
        srvHeap->GetCPUDescriptorHandleForHeapStart()
    );

    #pragma region root signature
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = 0; // Corresponds to t0
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParams[2] = {};

    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].Descriptor.ShaderRegister = 0;
    rootParams[0].Descriptor.RegisterSpace = 0;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges = &srvRange;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.ShaderRegister = 0; // Corresponds to s0
    samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    ComPtr<ID3D12RootSignature> rootSignature;
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = 2;
    rootSignatureDesc.pParameters = rootParams;
    rootSignatureDesc.NumStaticSamplers = 1;
    rootSignatureDesc.pStaticSamplers = &samplerDesc;
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
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
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

        ID3D12DescriptorHeap* heaps[] = {srvHeap.Get()};
        context.GetCommandList()->SetDescriptorHeaps(1, heaps);

        context.GetCommandList()->SetGraphicsRootDescriptorTable(1, srvHeap->GetGPUDescriptorHandleForHeapStart());

        D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = constantBuffer->GetGPUVirtualAddress();
        context.GetCommandList()->SetGraphicsRootConstantBufferView(0, cbvAddress);

        context.GetCommandList()->ClearDepthStencilView(
            dsvHeap->GetCPUDescriptorHandleForHeapStart(),
            D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr
        );

        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
        context.GetCommandList()->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

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