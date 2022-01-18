#ifndef __DXWINDOW_H__
#define __DXWINDOW_H__

#include "stdafx.h"
#include "SwapChain.h"
#include "CommandQueue.h"
#include "DescriptorHeap.h"

using namespace DirectX;


struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT4 color;
};

class DXWindow
{
    const wchar_t* m_Name;
    std::wstring m_AssetsPath;

    // The number of swap chain back buffers.
    // static constexpr uint8_t m_NumFrames = 3;
    // Use WARP adapter
    bool m_UseWarp = false;

    uint32_t m_Width;
    uint32_t m_Height;

    // Set to true once the DX12 objects have been initialized.
    bool m_IsInitialized = false;

    // Window handle.
    HWND m_hWnd = nullptr;
    // Window rectangle (used to toggle fullscreen state).
    RECT m_WindowRect;

    // DirectX 12 Objects
    ComPtr<ID3D12Device2> m_device;

    std::shared_ptr<SwapChain> m_swapChain;
    std::shared_ptr<CommandQueue> m_commandQueue;
    std::shared_ptr<RTVDescriptorHeap> m_RTVHeap;
    std::shared_ptr<DSVDescriptorHeap> m_DSVHeap;
    // ComPtr<ID3D12CommandQueue> m_CommandQueue;
    // ComPtr<IDXGISwapChain4> m_SwapChain;
    // ComPtr<ID3D12Resource> m_BackBuffers[m_NumFrames];
    // ComPtr<ID3D12GraphicsCommandList2> m_CommandList;
    // // 使用多个allocator的原因：在创建新的command list时，绑定新的资源需要释放allocator中的资源，
    // // 而当前渲染帧如果没有结束，则资源仍然需要，不能被释放，command list只是容器作用，提交到queue之后就可以重置。
    // ComPtr<ID3D12CommandAllocator> m_CommandAllocators[m_NumFrames];
    // UINT m_CurrentBackBufferIndex = 0;

    ComPtr<ID3D12RootSignature> m_RootSignature;
    ComPtr<ID3D12PipelineState> m_PipelineState;
    ComPtr<ID3D12Resource> m_VertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;
    ComPtr<ID3D12Resource> m_IndexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;

    // Depth buffer.
    ComPtr<ID3D12Resource> m_DepthBuffer;
    // Descriptor heap for depth buffer.
    // ComPtr<ID3D12DescriptorHeap> m_DSVHeap;
    // ComPtr<ID3D12DescriptorHeap> m_RTVDescriptorHeap;
    // UINT m_RTVDescriptorSize = 0;

    CD3DX12_VIEWPORT m_Viewport;
    CD3DX12_RECT m_ScissorRect;

    // Synchronization objects
    // ComPtr<ID3D12Fence> m_Fence;
    // uint64_t m_FenceValue = 0;
    // uint64_t m_FrameFenceValues[m_NumFrames] = {};
    // HANDLE m_FenceEvent = nullptr;

    // By default, enable V-Sync.
    // Can be toggled with the V key.
    // bool m_VSync = true;
    // bool m_TearingSupported = false;
    // By default, use windowed mode.
    // Can be toggled with the Alt+Enter or F11
    bool m_Fullscreen = false;

    float m_FoV;

    DirectX::XMMATRIX m_ModelMatrix;
    DirectX::XMMATRIX m_ViewMatrix;
    DirectX::XMMATRIX m_ProjectionMatrix;

    static bool CheckTearingSupport();

    std::wstring GetAssetFullPath(LPCWSTR assetName);

#pragma region LoadPipelineFunction
    void LoadPipeline();
    ComPtr<IDXGIAdapter4> GetAdapter() const;
    void CreateDevice();
    void CreateCommandQueue(D3D12_COMMAND_LIST_TYPE type);
    void CreateSwapChain();
    void CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type);
    void CreateCommandList(D3D12_COMMAND_LIST_TYPE type, ID3D12PipelineState* pipelineState);
    void CreateFence();
#pragma endregion
#pragma region LoadAssetsFunction
    void UpdateBufferResource(
        ComPtr<ID3D12GraphicsCommandList> commandList,
        ID3D12Resource** pDestinationResource,
        ID3D12Resource** pIntermediateResource,
        size_t numElements, size_t elementSize, const void* bufferData,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
    void LoadAssets();
#pragma endregion

    void UpdateRenderTargetViews();
public:
    DXWindow(const wchar_t* name, uint32_t w = 1280, uint32_t h = 720) noexcept;
    ~DXWindow() = default;

    void ParseCommandLineArguments();
    void Init(HWND hWnd);
    void Update();
    void Render();
    void Resize(uint32_t width, uint32_t height);
    void ResizeDepthBuffer(uint32_t width, uint32_t height);
    void Destroy();

    LRESULT OnWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    void SetFullscreen(bool fullscreen);
    void SetVSync(bool VSync);

    HWND GetHandler() const;
    uint32_t GetWidth() const;
    uint32_t GetHeight() const;
    const wchar_t* GetName() const;

    bool IsInitialized() const;
    bool IsFullscreen() const;
    bool IsVSync() const;
};

#endif