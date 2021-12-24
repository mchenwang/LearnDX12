#ifndef __DXWINDOW_H__
#define __DXWINDOW_H__

#include "stdafx.h"

class DXWindow
{
    const wchar_t* m_Name;

    // The number of swap chain back buffers.
    static constexpr uint8_t m_NumFrames = 3;
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
    ComPtr<ID3D12Device2> m_Device;
    ComPtr<ID3D12CommandQueue> m_CommandQueue;
    ComPtr<IDXGISwapChain4> m_SwapChain;
    ComPtr<ID3D12Resource> m_BackBuffers[m_NumFrames];
    ComPtr<ID3D12GraphicsCommandList> m_CommandList;
    ComPtr<ID3D12CommandAllocator> m_CommandAllocators[m_NumFrames];
    ComPtr<ID3D12DescriptorHeap> m_RTVDescriptorHeap;
    UINT m_RTVDescriptorSize = 0;
    UINT m_CurrentBackBufferIndex = 0;

    // Synchronization objects
    ComPtr<ID3D12Fence> m_Fence;
    uint64_t m_FenceValue = 0;
    uint64_t m_FrameFenceValues[m_NumFrames] = {};
    HANDLE m_FenceEvent = nullptr;

    // By default, enable V-Sync.
    // Can be toggled with the V key.
    bool m_VSync = true;
    bool m_TearingSupported = false;
    // By default, use windowed mode.
    // Can be toggled with the Alt+Enter or F11
    bool m_Fullscreen = false;

    static bool CheckTearingSupport();
    ComPtr<IDXGIAdapter4> GetAdapter() const;
    void CreateDevice();
    void CreateCommandQueue(D3D12_COMMAND_LIST_TYPE type);
    void CreateSwapChain();
    void CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type);
    void UpdateRenderTargetViews();
    void CreateCommandList(D3D12_COMMAND_LIST_TYPE type);
    void CreateFence();
public:
    DXWindow(const wchar_t* name, uint32_t w = 1280, uint32_t h = 720) noexcept;
    ~DXWindow() = default;

    void ParseCommandLineArguments();
    void Init(HWND hWnd);
    void Update();
    void Render();
    void Resize(uint32_t, uint32_t);
    void Destroy();

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