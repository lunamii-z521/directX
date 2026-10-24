#pragma once

struct D3D12VertexBuffer_InputDesc_Elem
{
    std::string SemanticsName;
    DXGI_FORMAT Format;
};

class D3D12VertexBuffer_InputDesc
{
public:
    D3D12VertexBuffer_InputDesc() = default;
    D3D12VertexBuffer_InputDesc(std::initializer_list<D3D12VertexBuffer_InputDesc_Elem> list);

    D3D12VertexBuffer_InputDesc(const D3D12VertexBuffer_InputDesc& other);
    D3D12VertexBuffer_InputDesc& operator=(const D3D12VertexBuffer_InputDesc& other);

    size_t GetStride() const;
    D3D12_INPUT_LAYOUT_DESC GetInputLayoutDesc() const;

private:
    void RebuildElementDescs();

    // Store persistent elements and strings so pointers never dangle
    std::vector<std::string> m_semanticNames;
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_elementDescs;
};

class D3D12VertexBuffer
{
public:
    D3D12VertexBuffer() = default;
    D3D12VertexBuffer(float* data, size_t size, const D3D12VertexBuffer_InputDesc& input_desc);
    ~D3D12VertexBuffer() = default;

    bool Init(float* data, size_t size, const D3D12VertexBuffer_InputDesc& input_desc);

    D3D12_INPUT_LAYOUT_DESC GetInputDesc() const { return m_input_desc_storage.GetInputLayoutDesc(); }
    D3D12_VERTEX_BUFFER_VIEW GetVBView() const { return m_vb_view; }

private:
    ComPtr<ID3D12Resource> m_vertexBuffer;
    ComPtr<ID3D12Resource> m_vertexBufferUpload;
    D3D12VertexBuffer_InputDesc m_input_desc_storage;
    D3D12_VERTEX_BUFFER_VIEW m_vb_view = {};
};