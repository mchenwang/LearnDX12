#ifndef __DESCRIPTORHEAP_H__
#define __DESCRIPTORHEAP_H__

#include <d3d12.h>
#include <wrl.h>
#include "d3dx12.h"
using Microsoft::WRL::ComPtr;

class DescriptorHeap
{
protected:
    ComPtr<ID3D12DescriptorHeap> m_heap;
    UINT m_numDescriptors;
    UINT m_descriptorSize;
    DescriptorHeap(ComPtr<ID3D12Device2> device, UINT num, D3D12_DESCRIPTOR_HEAP_TYPE type,
        D3D12_DESCRIPTOR_HEAP_FLAGS flag);
public:
    virtual D3D12_DESCRIPTOR_HEAP_TYPE GetType() const = 0;
    CD3DX12_CPU_DESCRIPTOR_HANDLE GetCPUHeapStartPtr(UINT index = 0) const;
    CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUHeapStartPtr(UINT index = 0) const;
    ComPtr<ID3D12DescriptorHeap> GetHeap() const;
};

class RTVDescriptorHeap : public DescriptorHeap
{
public:
    static const D3D12_DESCRIPTOR_HEAP_TYPE S_TYPE = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
public:
    RTVDescriptorHeap(ComPtr<ID3D12Device2> device, UINT num, D3D12_DESCRIPTOR_HEAP_FLAGS flag = D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    D3D12_DESCRIPTOR_HEAP_TYPE GetType() const;
};

class DSVDescriptorHeap : public DescriptorHeap
{
public:
    static const D3D12_DESCRIPTOR_HEAP_TYPE S_TYPE = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
public:
    DSVDescriptorHeap(ComPtr<ID3D12Device2> device, UINT num, D3D12_DESCRIPTOR_HEAP_FLAGS flag = D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    D3D12_DESCRIPTOR_HEAP_TYPE GetType() const;
};

class CBVSRVUAVDescriptorHeap : public DescriptorHeap
{
public:
    static const D3D12_DESCRIPTOR_HEAP_TYPE S_TYPE = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
public:
    CBVSRVUAVDescriptorHeap(ComPtr<ID3D12Device2> device, UINT num, D3D12_DESCRIPTOR_HEAP_FLAGS flag = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
    D3D12_DESCRIPTOR_HEAP_TYPE GetType() const;
};

using CBVDescriptorHeap = CBVSRVUAVDescriptorHeap;
using SRVDescriptorHeap = CBVSRVUAVDescriptorHeap;
using UAVDescriptorHeap = CBVSRVUAVDescriptorHeap;
// class CBVDescriptorHeap : public CBVSRVUAVDescriptorHeap
// {
// };
// class SRVDescriptorHeap : public CBVSRVUAVDescriptorHeap
// {
// };
// class UAVDescriptorHeap : public CBVSRVUAVDescriptorHeap
// {
// };

#endif