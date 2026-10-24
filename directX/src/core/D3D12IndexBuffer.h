#pragma once

class D3D12IndexBuffer
{
public:
    D3D12IndexBuffer() = default;
    D3D12IndexBuffer(uint32_t* data, size_t size);
    ~D3D12IndexBuffer() = default;

    bool Init(uint32_t* data, size_t size);

    D3D12_INDEX_BUFFER_VIEW GetIBView() const { return m_ib_view; }

private:
    ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_ib_view = {};
};

