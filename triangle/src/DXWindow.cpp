#include "DXWindow.h"
#include "helper.h"
#include <chrono>
#include <dxgidebug.h>

static uint64_t Signal(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence,
    uint64_t& fenceValue)
{
    uint64_t fenceValueForSignal = ++fenceValue;
    ThrowIfFailed(commandQueue->Signal(fence.Get(), fenceValueForSignal));

    return fenceValueForSignal;
}

static void WaitForFenceValue(ComPtr<ID3D12Fence> fence, uint64_t fenceValue, HANDLE fenceEvent,
    std::chrono::milliseconds duration = std::chrono::milliseconds::max() )
{
    if (fence->GetCompletedValue() < fenceValue)
    {
        ThrowIfFailed(fence->SetEventOnCompletion(fenceValue, fenceEvent));
        ::WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count()));
    }
}

static void Flush(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence,
    uint64_t& fenceValue, HANDLE fenceEvent )
{
    uint64_t fenceValueForSignal = Signal(commandQueue, fence, fenceValue);
    WaitForFenceValue(fence, fenceValueForSignal, fenceEvent);
}

DXWindow::DXWindow(const wchar_t* name, uint32_t w, uint32_t h) noexcept
    : m_Name(name)
    , m_Width(w)
    , m_Height(h)
    , m_Viewport(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h))
    , m_ScissorRect(0, 0, static_cast<LONG>(w), static_cast<LONG>(h))
{
    m_AssetsPath = shader_path;
}

void DXWindow::ParseCommandLineArguments()
{
    int argc;
    wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
 
    for (size_t i = 0; i < argc; ++i)
    {
        if (::wcscmp(argv[i], L"-w") == 0 || ::wcscmp(argv[i], L"--width") == 0)
        {
            m_Width = ::wcstol(argv[++i], nullptr, 10);
        }
        if (::wcscmp(argv[i], L"-h") == 0 || ::wcscmp(argv[i], L"--height") == 0)
        {
            m_Height = ::wcstol(argv[++i], nullptr, 10);
        }
        if (::wcscmp(argv[i], L"-warp") == 0 || ::wcscmp(argv[i], L"--warp") == 0)
        {
            m_UseWarp = true;
        }
    }
 
    // Free memory allocated by CommandLineToArgvW
    ::LocalFree(argv);
}

bool DXWindow::CheckTearingSupport()
{
    BOOL allowTearing = FALSE;

    // Rather than create the DXGI 1.5 factory interface directly, we create the
    // DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
    // graphics debugging tools which will not support the 1.5 factory interface 
    // until a future update.
    ComPtr<IDXGIFactory4> factory4;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
    {
        ComPtr<IDXGIFactory5> factory5;
        if (SUCCEEDED(factory4.As(&factory5)))
        {
            if (FAILED(factory5->CheckFeatureSupport(
                DXGI_FEATURE_PRESENT_ALLOW_TEARING, 
                &allowTearing, sizeof(allowTearing))))
            {
                allowTearing = FALSE;
            }
        }
    }

    return allowTearing == TRUE;
}

std::wstring DXWindow::GetAssetFullPath(LPCWSTR assetName)
{
    return m_AssetsPath + assetName;
}

ComPtr<IDXGIAdapter4> DXWindow::GetAdapter() const
{
    ComPtr<IDXGIFactory4> dxgiFactory;
    UINT createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

    ComPtr<IDXGIAdapter1> dxgiAdapter1;
    ComPtr<IDXGIAdapter4> dxgiAdapter4;

    if (m_UseWarp)
    {
        ThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
        ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
    }
    else
    {
        ComPtr<IDXGIFactory6> factory6;
        if (SUCCEEDED(dxgiFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
        {
            for (
                UINT adapterIndex = 0;
                SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                    adapterIndex,
                    DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, // Find adapter with the highest performance
                    IID_PPV_ARGS(&dxgiAdapter1)));
                ++adapterIndex)
            {
                DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
                dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);
                if (dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    // Don't select the Basic Render Driver adapter.
                    // If you want a software adapter, pass in "/warp" on the command line.
                    continue;
                }

                // Check to see whether the adapter supports Direct3D 12, but don't create the
                // actual device yet.
                if (SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
                {
                    ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
                    break;
                }
            }
        }
    }

    return dxgiAdapter4;
}


void DXWindow::CreateDevice()
{
    ComPtr<IDXGIAdapter4> adapter = GetAdapter();
    ComPtr<ID3D12Device2> d3d12Device2;
    ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2)));
    // Enable debug messages in debug mode.
#if defined(_DEBUG)
    ComPtr<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(d3d12Device2.As(&pInfoQueue)))
    {
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
        // Suppress whole categories of messages
        //D3D12_MESSAGE_CATEGORY Categories[] = {};

        // Suppress messages based on their severity level
        D3D12_MESSAGE_SEVERITY Severities[] =
        {
            D3D12_MESSAGE_SEVERITY_INFO
        };

        // Suppress individual messages by their ID
        D3D12_MESSAGE_ID DenyIds[] = {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
            
            // Workarounds for debug layer issues on hybrid-graphics systems
            D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_WRONGSWAPCHAINBUFFERREFERENCE,
            D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
        };

        D3D12_INFO_QUEUE_FILTER NewFilter = {};
        //NewFilter.DenyList.NumCategories = _countof(Categories);
        //NewFilter.DenyList.pCategoryList = Categories;
        NewFilter.DenyList.NumSeverities = _countof(Severities);
        NewFilter.DenyList.pSeverityList = Severities;
        NewFilter.DenyList.NumIDs = _countof(DenyIds);
        NewFilter.DenyList.pIDList = DenyIds;

        ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
    }
#endif

    m_Device = d3d12Device2;
}

void DXWindow::CreateCommandQueue(D3D12_COMMAND_LIST_TYPE type)
{
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type =     type;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags =    D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    ThrowIfFailed(m_Device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_CommandQueue)));

    // create command allocator
    for (int i = 0; i < m_NumFrames; ++i)
    {
        ThrowIfFailed(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocators[i])));
    }
}

void DXWindow::CreateSwapChain()
{
    ComPtr<IDXGIFactory4> dxgiFactory4;
    UINT createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = m_Width;
    swapChainDesc.Height = m_Height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc = { 1, 0 };
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = m_NumFrames;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    // It is recommended to always allow tearing if tearing support is available.
    swapChainDesc.Flags = m_TearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;;
    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(
        m_CommandQueue.Get(),
        m_hWnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1));

    // Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
    // will be handled manually.
    ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER));
    ThrowIfFailed(swapChain1.As(&m_SwapChain));

    m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
}

void DXWindow::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = m_NumFrames;
    desc.Type = type;

    ThrowIfFailed(m_Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_RTVDescriptorHeap)));
    m_RTVDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(type);
}

void DXWindow::UpdateRenderTargetViews()
{
    auto rtvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    for (int i = 0; i < m_NumFrames; ++i)
    {
        ComPtr<ID3D12Resource> backBuffer;
        ThrowIfFailed(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));
        m_Device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);
        m_BackBuffers[i] = backBuffer;
        rtvHandle.Offset(rtvDescriptorSize);
    }
}

void DXWindow::CreateCommandList(D3D12_COMMAND_LIST_TYPE type, ID3D12PipelineState* pipelineState)
{
    ThrowIfFailed(m_Device->CreateCommandList(0, type, 
        m_CommandAllocators[m_CurrentBackBufferIndex].Get(), 
        pipelineState, IID_PPV_ARGS(&m_CommandList)));
    
    ThrowIfFailed(m_CommandList->Close());
}

void DXWindow::CreateFence()
{
    ThrowIfFailed(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence)));
    m_FenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    if (m_FenceEvent == nullptr)
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
}

/* 管线初始化步骤：
 *   1. 创建 device
 *   2. 创建 CommandQueue
 *   3. 创建 swap chain
 *   4. 创建描述符 heap
 *   5. 创建 RTV
 *   6. 创建 同步变量
 */
void DXWindow::LoadPipeline()
{
    CreateDevice();
    CreateCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
    CreateSwapChain();
    CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    UpdateRenderTargetViews(); // create RTV
    CreateFence();
}

/* 资源初始化步骤：
 *   1. 创建 root signature
 *   2. 创建 pipeline state，编译导入 Shader
 *   [3.] 创建 command list，可以和 command queue 同时创建
 *   4. 创建 vertex buffer
 */
void DXWindow::LoadAssets()
{
    // 1.
    {
        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        ThrowIfFailed(m_Device->CreateRootSignature(
            0,
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(&m_RootSignature)));
    }

    // 2.
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

        ThrowIfFailed(D3DCompileFromFile(
            GetAssetFullPath(L"shaders.hlsl").c_str(),
            nullptr, nullptr, "VSMain", "vs_5_0",
            compileFlags, 0, &vertexShader, nullptr
            )
        );
        ThrowIfFailed(D3DCompileFromFile(
            GetAssetFullPath(L"shaders.hlsl").c_str(),
            nullptr, nullptr, "PSMain", "ps_5_0",
            compileFlags, 0, &pixelShader, nullptr
            )
        );

        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = m_RootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;
        ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineState)));
    }

    // 3. 根据pipeline state创建command list可减少CPU开销
    CreateCommandList(D3D12_COMMAND_LIST_TYPE_DIRECT, m_PipelineState.Get());
    // 或者先使用为null的pipeline state创建command list，再set pipeline state
    // m_CommandList->SetPipelineState(m_PipelineState.Get());

    // 4.
    {
        Vertex triangleVertices[] =
        {
            { { 0.0f, 0.25f, 0.0f }, { 1.0f, .0f, .0f, 1.0f } },
            { { 0.25f, -0.25f, 0.0f }, { .0f, 1.0f, .0f, 1.0f } },
            { { -0.25f, -0.25f, 0.0f }, { .0f, .0f, 1.0f, 1.0f } }
        };
        const UINT vertexBufferSize = sizeof(triangleVertices);
        ThrowIfFailed(m_Device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_VertexBuffer)));
            
        // Copy the triangle data to the vertex buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_VertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
        m_VertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
        m_VertexBufferView.StrideInBytes = sizeof(Vertex);
        m_VertexBufferView.SizeInBytes = vertexBufferSize;
    }

    // {
    //     m_FrameFenceValues[m_CurrentBackBufferIndex] = Signal(m_CommandQueue, m_Fence, m_FenceValue);
    //     m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

    //     WaitForFenceValue(m_Fence, m_FrameFenceValues[m_CurrentBackBufferIndex], m_FenceEvent);
    // }
}

void DXWindow::Init(HWND hWnd)
{
    m_hWnd = hWnd;
    
    m_VSync = false;
    m_TearingSupported = CheckTearingSupport();
    // Initialize the global window rect variable.
    ::GetWindowRect(m_hWnd, &m_WindowRect);

    LoadPipeline();
    LoadAssets();
    
    m_IsInitialized = true;
}

void DXWindow::Destroy()
{
    Flush(m_CommandQueue, m_Fence, m_FenceValue, m_FenceEvent);
    ::CloseHandle(m_FenceEvent);
}

HWND DXWindow::GetHandler() const
{
    return m_hWnd;
}
uint32_t DXWindow::GetWidth() const
{
    return m_Width;
}
uint32_t DXWindow::GetHeight() const
{
    return m_Height;
}
const wchar_t* DXWindow::GetName() const
{
    return m_Name;
}
bool DXWindow::IsInitialized() const
{
    return m_IsInitialized;
}
void DXWindow::SetVSync(bool VSync)
{
    m_VSync = VSync;
}
bool DXWindow::IsVSync() const
{
    return m_VSync;
}
bool DXWindow::IsFullscreen() const
{
    return m_Fullscreen;
}

void DXWindow::Update()
{
    static uint64_t frameCounter = 0;
    static double elapsedSeconds = 0.0;
    static std::chrono::high_resolution_clock clock;
    static auto t0 = clock.now();

    frameCounter++;
    auto t1 = clock.now();
    auto deltaTime = t1 - t0;
    t0 = t1;
    elapsedSeconds += deltaTime.count() * 1e-9;
    if (elapsedSeconds > 1.0)
    {
        char buffer[500];
        auto fps = frameCounter / elapsedSeconds;
        sprintf_s(buffer, 500, "FPS: %f\n", fps);
        // OutputDebugStringA(buffer);
        // cout << buffer << "\n";
        SetWindowTextA(m_hWnd, buffer);
        frameCounter = 0;
        elapsedSeconds = 0.0;
    }
}

void DXWindow::Render()
{
    auto commandAllocator = m_CommandAllocators[m_CurrentBackBufferIndex];
    auto backBuffer = m_BackBuffers[m_CurrentBackBufferIndex];

    commandAllocator->Reset();
    m_CommandList->Reset(commandAllocator.Get(), m_PipelineState.Get());
    
    // Set necessary state.
    m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());
    m_CommandList->RSSetViewports(1, &m_Viewport);
    m_CommandList->RSSetScissorRects(1, &m_ScissorRect);
        
    // Clear the render target.
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer.Get(),
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        m_CommandList->ResourceBarrier(1, &barrier);

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(m_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            m_CurrentBackBufferIndex, m_RTVDescriptorSize);
        m_CommandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

        FLOAT clearColor[] = { 0.f, 0.f, 0.f, 1.0f };
        m_CommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    }
    // Set obj
    m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_CommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
    m_CommandList->DrawInstanced(3, 1, 0, 0);

    // Present
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        m_CommandList->ResourceBarrier(1, &barrier);
        ThrowIfFailed(m_CommandList->Close());

        ID3D12CommandList* const commandLists[] = {
            m_CommandList.Get()
        };
        m_CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
        UINT syncInterval = m_VSync ? 1 : 0;
        UINT presentFlags = m_TearingSupported && !m_VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
        ThrowIfFailed(m_SwapChain->Present(syncInterval, presentFlags));

        m_FrameFenceValues[m_CurrentBackBufferIndex] = Signal(m_CommandQueue, m_Fence, m_FenceValue);
        m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

        WaitForFenceValue(m_Fence, m_FrameFenceValues[m_CurrentBackBufferIndex], m_FenceEvent);
    }
}

void DXWindow::SetFullscreen(bool fullscreen)
{
    if (m_Fullscreen != fullscreen)
    {
        m_Fullscreen = fullscreen;

        if (m_Fullscreen) // Switching to fullscreen.
        {
            // Store the current window dimensions so they can be restored 
            // when switching out of fullscreen state.
            ::GetWindowRect(m_hWnd, &m_WindowRect);
            // Set the window style to a borderless window so the client area fills
            // the entire screen.
            UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

            ::SetWindowLongW(m_hWnd, GWL_STYLE, windowStyle);
            // Query the name of the nearest display device for the window.
            // This is required to set the fullscreen dimensions of the window
            // when using a multi-monitor setup.
            HMONITOR hMonitor = ::MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFOEX monitorInfo = {};
            monitorInfo.cbSize = sizeof(MONITORINFOEX);
            ::GetMonitorInfo(hMonitor, &monitorInfo);
            ::SetWindowPos(m_hWnd, HWND_TOP,
                monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.top,
                monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);

            ::ShowWindow(m_hWnd, SW_MAXIMIZE);
        }
        else
        {
            // Restore all the window decorators.
            ::SetWindowLongW(m_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

            ::SetWindowPos(m_hWnd, HWND_NOTOPMOST,
                m_WindowRect.left,
                m_WindowRect.top,
                m_WindowRect.right - m_WindowRect.left,
                m_WindowRect.bottom - m_WindowRect.top,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);

            ::ShowWindow(m_hWnd, SW_NORMAL);
        }
    }
}

void DXWindow::Resize(uint32_t width, uint32_t height)
{
    if (m_Width != width || m_Height != height)
    {
        // Don't allow 0 size swap chain back buffers.
        m_Width = std::max(1u, width );
        m_Height = std::max( 1u, height);

        // Flush the GPU queue to make sure the swap chain's back buffers
        // are not being referenced by an in-flight command list.
        Flush(m_CommandQueue, m_Fence, m_FenceValue, m_FenceEvent);
        for (int i = 0; i < m_NumFrames; ++i)
        {
            // Any references to the back buffers must be released
            // before the swap chain can be resized.
            m_BackBuffers[i].Reset();
            m_FrameFenceValues[i] = m_FrameFenceValues[m_CurrentBackBufferIndex];
        }
        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        ThrowIfFailed(m_SwapChain->GetDesc(&swapChainDesc));
        ThrowIfFailed(m_SwapChain->ResizeBuffers(m_NumFrames, m_Width, m_Height,
            swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

        m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

        UpdateRenderTargetViews();
    }
}