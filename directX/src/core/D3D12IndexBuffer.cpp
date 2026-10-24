#include "pch.h"
#include "D3D12IndexBuffer.h"

#include "D3D12Context.h"
extern D3D12Context context;

D3D12IndexBuffer::D3D12IndexBuffer(uint32_t* data, size_t size) { Init(data, size); }

bool D3D12IndexBuffer::Init(uint32_t* data, size_t size)
{
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC ibDesc = {};
    ibDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    ibDesc.Width = size;
    ibDesc.Height = 1;
    ibDesc.DepthOrArraySize = 1;
    ibDesc.MipLevels = 1;
    ibDesc.Format = DXGI_FORMAT_UNKNOWN;
    ibDesc.SampleDesc.Count = 1;
    ibDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    context.GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &ibDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_indexBuffer)
    );
    UINT8* pIndexDataBegin = nullptr;
    D3D12_RANGE ibreadRange = {0, 0};
    m_indexBuffer->Map(0, &ibreadRange, reinterpret_cast<void**>(&pIndexDataBegin));
    memcpy(pIndexDataBegin, data, size);
    m_indexBuffer->Unmap(0, nullptr);
    m_ib_view.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_ib_view.SizeInBytes = size;
    m_ib_view.Format = DXGI_FORMAT_R32_UINT;

    return true;
}