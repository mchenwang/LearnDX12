#include "DescriptorHeap.h"
#include "helper.h"

DescriptorHeap::DescriptorHeap(ComPtr<ID3D12Device2> device, UINT num, D3D12_DESCRIPTOR_HEAP_TYPE type,
        D3D12_DESCRIPTOR_HEAP_FLAGS flag)
    : m_numDescriptors(num)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = num;
    desc.Type = type;
    desc.Flags = flag;
    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap)));

    m_descriptorSize = device->GetDescriptorHandleIncrementSize(type);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCPUHeapStartPtr(UINT index) const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_heap->GetCPUDescriptorHandleForHeapStart(), index, m_descriptorSize);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetGPUHeapStartPtr(UINT index) const
{
    return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_heap->GetGPUDescriptorHandleForHeapStart(), index, m_descriptorSize);
}

RTVDescriptorHeap::RTVDescriptorHeap(ComPtr<ID3D12Device2> device, UINT num,
        D3D12_DESCRIPTOR_HEAP_FLAGS flag)
    : DescriptorHeap(device, num, S_TYPE, flag)
{
}

DSVDescriptorHeap::DSVDescriptorHeap(ComPtr<ID3D12Device2> device, UINT num,
        D3D12_DESCRIPTOR_HEAP_FLAGS flag)
    : DescriptorHeap(device, num, S_TYPE, flag)
{
}

CBVSRVUAVDescriptorHeap::CBVSRVUAVDescriptorHeap(ComPtr<ID3D12Device2> device, UINT num,
        D3D12_DESCRIPTOR_HEAP_FLAGS flag)
    : DescriptorHeap(device, num, S_TYPE, flag)
{
}

ComPtr<ID3D12DescriptorHeap> DescriptorHeap::GetHeap() const
{
    return m_heap;
}

D3D12_DESCRIPTOR_HEAP_TYPE RTVDescriptorHeap::GetType() const
{
    return S_TYPE;
}
D3D12_DESCRIPTOR_HEAP_TYPE DSVDescriptorHeap::GetType() const
{
    return S_TYPE;
}
D3D12_DESCRIPTOR_HEAP_TYPE CBVSRVUAVDescriptorHeap::GetType() const
{
    return S_TYPE;
}