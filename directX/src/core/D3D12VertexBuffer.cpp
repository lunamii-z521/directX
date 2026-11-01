#include "pch.h"
#include "D3D12VertexBuffer.h"

#include "D3D12Context.h"
extern D3D12Context context;

D3D12VertexBuffer_InputDesc::D3D12VertexBuffer_InputDesc(std::initializer_list<D3D12VertexBuffer_InputDesc_Elem> list)
{
    for (auto& elem : list) m_semanticNames.push_back(elem.SemanticsName);
    
    uint32_t idx = 0;
    for (auto& elem : list)
    {
        D3D12_INPUT_ELEMENT_DESC d3dElem = {};
        d3dElem.SemanticName = m_semanticNames[idx].c_str();
        d3dElem.SemanticIndex = 0;
        d3dElem.Format = elem.Format;
        d3dElem.InputSlot = 0;
        d3dElem.AlignedByteOffset = (idx == 0) ? 0 : D3D12_APPEND_ALIGNED_ELEMENT;
        d3dElem.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        d3dElem.InstanceDataStepRate = 0;

        m_elementDescs.push_back(d3dElem);
        idx++;
    }
}
D3D12VertexBuffer_InputDesc::D3D12VertexBuffer_InputDesc(const D3D12VertexBuffer_InputDesc& other) { CopyElementDescs(other); }

// Custom Copy Assignment Operator
D3D12VertexBuffer_InputDesc& D3D12VertexBuffer_InputDesc::operator=(const D3D12VertexBuffer_InputDesc& other)
{
    if (this != &other) CopyElementDescs(other);
    return *this;
}

void D3D12VertexBuffer_InputDesc::CopyElementDescs(const D3D12VertexBuffer_InputDesc& other)
{
    m_elementDescs.clear();
    m_semanticNames = other.m_semanticNames;
    uint32_t idx = 0;
    for (size_t i = 0; i < m_semanticNames.size(); ++i)
    {
        D3D12_INPUT_ELEMENT_DESC d3dElem = {};
        d3dElem.SemanticName = m_semanticNames[i].c_str();
        d3dElem.SemanticIndex = other.m_elementDescs[i].SemanticIndex;

        d3dElem.Format = other.m_elementDescs[i].Format;
        d3dElem.InputSlot = other.m_elementDescs[i].InputSlot;
        d3dElem.AlignedByteOffset = other.m_elementDescs[i].AlignedByteOffset;
        d3dElem.InputSlotClass = other.m_elementDescs[i].InputSlotClass;
        d3dElem.InstanceDataStepRate = other.m_elementDescs[i].InstanceDataStepRate;

        m_elementDescs.push_back(d3dElem);
        idx++;
    }
}

D3D12_INPUT_LAYOUT_DESC D3D12VertexBuffer_InputDesc::GetInputLayoutDesc() const
{
    D3D12_INPUT_LAYOUT_DESC desc = {};
    desc.pInputElementDescs = m_elementDescs.data();
    desc.NumElements = static_cast<UINT>(m_elementDescs.size());
    return desc;
}

D3D12VertexBuffer::D3D12VertexBuffer(float* data, size_t size, const D3D12VertexBuffer_InputDesc& input_desc) { Init(data, size, input_desc); }

size_t D3D12VertexBuffer_InputDesc::GetStride() const
{
    size_t stride = 0;
    for (const auto& elem : m_elementDescs)
    {
        switch (elem.Format)
        {
        case DXGI_FORMAT_R32G32_FLOAT:      stride += 8;  break;
        case DXGI_FORMAT_R32G32B32_FLOAT:   stride += 12; break;
        case DXGI_FORMAT_R32G32B32A32_FLOAT:stride += 16; break;
        default: break;
        }
    }
    return stride;
}

bool D3D12VertexBuffer::Init(float* data, size_t size, const D3D12VertexBuffer_InputDesc& input_desc)
{
    m_input_desc_storage = input_desc;

    D3D12_HEAP_PROPERTIES heapProps = {};
    D3D12_HEAP_PROPERTIES heapProps_def = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps_def.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = size;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    // Create Default Heap (VRAM)
    context.GetDevice()->CreateCommittedResource(
        &heapProps_def,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_vertexBuffer)
    );

    // Create Upload Heap (Staging)
    context.GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_vertexBufferUpload)
    );

    // Map and copy data into upload heap
    UINT8* pVertexDataBegin = nullptr;
    D3D12_RANGE vbreadRange = {0, 0};
    m_vertexBufferUpload->Map(0, &vbreadRange, reinterpret_cast<void**>(&pVertexDataBegin));
    memcpy(pVertexDataBegin, data, size);
    m_vertexBufferUpload->Unmap(0, nullptr);

    // Copy from upload heap to default VRAM heap
    context.GetCommandList()->CopyBufferRegion(
        m_vertexBuffer.Get(), 0,
        m_vertexBufferUpload.Get(), 0,
        size
    );

    // Transition default heap from COPY_DEST -> VERTEX_AND_CONSTANT_BUFFER
    D3D12_RESOURCE_BARRIER vbBarrier = {};
    vbBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    vbBarrier.Transition.pResource = m_vertexBuffer.Get();
    vbBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    vbBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    vbBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    context.GetCommandList()->ResourceBarrier(1, &vbBarrier);

    // Assign member variables correctly
    m_vb_view.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vb_view.StrideInBytes = static_cast<UINT>(input_desc.GetStride());
    m_vb_view.SizeInBytes = static_cast<UINT>(size);

    return true;
}