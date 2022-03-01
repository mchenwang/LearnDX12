#include "DXWindow.h"
#include "helper.h"
#include <chrono>
#include <dxgidebug.h>
#include <algorithm>
#include "Application.h"

#include <wincodec.h>   //for WIC
#include <cmath> // for ceil

DXWindow::DXWindow(const wchar_t* name, uint32_t w, uint32_t h) noexcept
    : m_name(name)
    , m_width(w)
    , m_height(h)
    , m_viewport(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h))
    , m_scissorRect(0, 0, static_cast<LONG>(w), static_cast<LONG>(h))
{
    m_assetsPath = project_path;
    m_camera = std::make_shared<PerspectiveCamera>(static_cast<float>(w), static_cast<float>(h));
}

void DXWindow::ParseCommandLineArguments()
{
    int argc;
    wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
 
    for (size_t i = 0; i < argc; ++i)
    {
        if (::wcscmp(argv[i], L"-w") == 0 || ::wcscmp(argv[i], L"--width") == 0)
        {
            m_width = ::wcstol(argv[++i], nullptr, 10);
        }
        if (::wcscmp(argv[i], L"-h") == 0 || ::wcscmp(argv[i], L"--height") == 0)
        {
            m_height = ::wcstol(argv[++i], nullptr, 10);
        }
        if (::wcscmp(argv[i], L"-warp") == 0 || ::wcscmp(argv[i], L"--warp") == 0)
        {
            m_useWarp = true;
        }
    }
 
    // Free memory allocated by CommandLineToArgvW
    ::LocalFree(argv);
}

std::wstring DXWindow::GetAssetFullPath(LPCWSTR assetName)
{
    return m_assetsPath + assetName;
}

std::wstring DXWindow::GetShaderFullPath(LPCWSTR assetName)
{
    return m_assetsPath + L"shader/" + assetName;
}

void DXWindow::LoadPipeline()
{
    m_device = Application::GetInstance()->GetDevice();
    m_commandQueue = std::make_shared<CommandQueue>(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
    m_swapChain = std::make_shared<SwapChain>(m_device, m_width, m_height, m_hWnd, m_commandQueue);
    m_RTVDescriptorHeap = std::make_shared<RTVDescriptorHeap>(m_device, SwapChain::NUM_OF_FRAMES);
    m_DSVDescriptorHeap = std::make_shared<DSVDescriptorHeap>(m_device, 2);
    m_SRVDescriptorHeap = std::make_shared<SRVDescriptorHeap>(m_device, 2);
    m_CBVDescriptorHeap = std::make_shared<CBVDescriptorHeap>(m_device, 2);
    
    m_swapChain->UpdateRenderTargetViews(m_RTVDescriptorHeap);
}

void DXWindow::UpdateBufferResource(
    ComPtr<ID3D12GraphicsCommandList> commandList,
    ID3D12Resource** pDestinationResource,
    ID3D12Resource** pIntermediateResource,
    size_t numElements, size_t elementSize, const void* bufferData,
    D3D12_RESOURCE_FLAGS flags)
{
    size_t bufferSize = numElements * elementSize;

    // Create a committed resource for the GPU resource in a default heap.
    ThrowIfFailed(m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags),
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(pDestinationResource)));

    // Create an committed resource for the upload.
    if (bufferData)
    {
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(pIntermediateResource)));

        D3D12_SUBRESOURCE_DATA subresourceData = {};
        subresourceData.pData = bufferData;
        subresourceData.RowPitch = bufferSize;
        subresourceData.SlicePitch = subresourceData.RowPitch;

        UpdateSubresources(commandList.Get(),
            *pDestinationResource, *pIntermediateResource,
            0, 0, 1, &subresourceData);
    }
}
struct MVPData
{
    XMMATRIX mvp;
    XMMATRIX modelMatrixNegaTrans;
    XMMATRIX modelMatrix;
};
MVPData g_MVPCB;
BYTE* g_gpuMVPCB = nullptr;
#define GRS_UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B - 1)))
constexpr UINT g_mvpSize = GRS_UPPER(sizeof(MVPData), 256);
struct PassData
{
    XMMATRIX lightVp;
    XMFLOAT4 lightPos = XMFLOAT4(-2.f, 2.f, 2.f, 1.f);
    XMFLOAT4 eyePos = XMFLOAT4(0.5f, 0.5f, 5.f, 1.f);
    XMFLOAT3 ambientColor = XMFLOAT3(0.5f, 0.5f, 0.5f);
    float lightIntensity = 2.f;
    XMFLOAT3 lightColor = XMFLOAT3(1.f, 1.f, 1.f);
    float spotPower = 128.f;
};
PassData g_passData;
BYTE* g_gpuPassData = nullptr;
constexpr UINT g_passDataSize = GRS_UPPER(sizeof(PassData), 256);

#pragma region WIC

struct WICTranslate
{
	GUID wic;
	DXGI_FORMAT format;
};

static WICTranslate g_WICFormats[] =
{//WIC格式与DXGI像素格式的对应表，该表中的格式为被支持的格式
	{ GUID_WICPixelFormat128bppRGBAFloat,       DXGI_FORMAT_R32G32B32A32_FLOAT },

	{ GUID_WICPixelFormat64bppRGBAHalf,         DXGI_FORMAT_R16G16B16A16_FLOAT },
	{ GUID_WICPixelFormat64bppRGBA,             DXGI_FORMAT_R16G16B16A16_UNORM },

	{ GUID_WICPixelFormat32bppRGBA,             DXGI_FORMAT_R8G8B8A8_UNORM },
	{ GUID_WICPixelFormat32bppBGRA,             DXGI_FORMAT_B8G8R8A8_UNORM }, // DXGI 1.1
	{ GUID_WICPixelFormat32bppBGR,              DXGI_FORMAT_B8G8R8X8_UNORM }, // DXGI 1.1

	{ GUID_WICPixelFormat32bppRGBA1010102XR,    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM }, // DXGI 1.1
	{ GUID_WICPixelFormat32bppRGBA1010102,      DXGI_FORMAT_R10G10B10A2_UNORM },

	{ GUID_WICPixelFormat16bppBGRA5551,         DXGI_FORMAT_B5G5R5A1_UNORM },
	{ GUID_WICPixelFormat16bppBGR565,           DXGI_FORMAT_B5G6R5_UNORM },

	{ GUID_WICPixelFormat32bppGrayFloat,        DXGI_FORMAT_R32_FLOAT },
	{ GUID_WICPixelFormat16bppGrayHalf,         DXGI_FORMAT_R16_FLOAT },
	{ GUID_WICPixelFormat16bppGray,             DXGI_FORMAT_R16_UNORM },
	{ GUID_WICPixelFormat8bppGray,              DXGI_FORMAT_R8_UNORM },

	{ GUID_WICPixelFormat8bppAlpha,             DXGI_FORMAT_A8_UNORM },
};

// WIC 像素格式转换表.
struct WICConvert
{
	GUID source;
	GUID target;
};

static WICConvert g_WICConvert[] =
{
	// 目标格式一定是最接近的被支持的格式
	{ GUID_WICPixelFormatBlackWhite,            GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM

	{ GUID_WICPixelFormat1bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat2bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat4bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat8bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM

	{ GUID_WICPixelFormat2bppGray,              GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM
	{ GUID_WICPixelFormat4bppGray,              GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM

	{ GUID_WICPixelFormat16bppGrayFixedPoint,   GUID_WICPixelFormat16bppGrayHalf }, // DXGI_FORMAT_R16_FLOAT
	{ GUID_WICPixelFormat32bppGrayFixedPoint,   GUID_WICPixelFormat32bppGrayFloat }, // DXGI_FORMAT_R32_FLOAT

	{ GUID_WICPixelFormat16bppBGR555,           GUID_WICPixelFormat16bppBGRA5551 }, // DXGI_FORMAT_B5G5R5A1_UNORM

	{ GUID_WICPixelFormat32bppBGR101010,        GUID_WICPixelFormat32bppRGBA1010102 }, // DXGI_FORMAT_R10G10B10A2_UNORM

	{ GUID_WICPixelFormat24bppBGR,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat24bppRGB,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat32bppPBGRA,            GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat32bppPRGBA,            GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM

	{ GUID_WICPixelFormat48bppRGB,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat48bppBGR,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppBGRA,             GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppPRGBA,            GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppPBGRA,            GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM

	{ GUID_WICPixelFormat48bppRGBFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat48bppBGRFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat64bppRGBAFixedPoint,   GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat64bppBGRAFixedPoint,   GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat64bppRGBFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat48bppRGBHalf,          GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat64bppRGBHalf,          GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT

	{ GUID_WICPixelFormat128bppPRGBAFloat,      GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
	{ GUID_WICPixelFormat128bppRGBFloat,        GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
	{ GUID_WICPixelFormat128bppRGBAFixedPoint,  GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
	{ GUID_WICPixelFormat128bppRGBFixedPoint,   GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
	{ GUID_WICPixelFormat32bppRGBE,             GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT

	{ GUID_WICPixelFormat32bppCMYK,             GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat64bppCMYK,             GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat40bppCMYKAlpha,        GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat80bppCMYKAlpha,        GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM

	{ GUID_WICPixelFormat32bppRGB,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat64bppRGB,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppPRGBAHalf,        GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
};

bool GetTargetPixelFormat(const GUID* pSourceFormat, GUID* pTargetFormat)
{//查表确定兼容的最接近格式是哪个
	*pTargetFormat = *pSourceFormat;
	for (size_t i = 0; i < _countof(g_WICConvert); ++i)
	{
		if (InlineIsEqualGUID(g_WICConvert[i].source, *pSourceFormat))
		{
			*pTargetFormat = g_WICConvert[i].target;
			return true;
		}
	}
	return false;
}

DXGI_FORMAT GetDXGIFormatFromPixelFormat(const GUID* pPixelFormat)
{//查表确定最终对应的DXGI格式是哪一个
	for (size_t i = 0; i < _countof(g_WICFormats); ++i)
	{
		if (InlineIsEqualGUID(g_WICFormats[i].wic, *pPixelFormat))
		{
			return g_WICFormats[i].format;
		}
	}
	return DXGI_FORMAT_UNKNOWN;
}

#pragma endregion

std::array<CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers()
{
	//过滤器POINT,寻址模式WRAP的静态采样器
	CD3DX12_STATIC_SAMPLER_DESC pointWarp(0,	//着色器寄存器
		D3D12_FILTER_MIN_MAG_MIP_POINT,		//过滤器类型为POINT(常量插值)
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,	//U方向上的寻址模式为WRAP（重复寻址模式）
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,	//V方向上的寻址模式为WRAP（重复寻址模式）
		D3D12_TEXTURE_ADDRESS_MODE_WRAP);	//W方向上的寻址模式为WRAP（重复寻址模式）

	//过滤器POINT,寻址模式CLAMP的静态采样器
	CD3DX12_STATIC_SAMPLER_DESC pointClamp(1,	//着色器寄存器
		D3D12_FILTER_MIN_MAG_MIP_POINT,		//过滤器类型为POINT(常量插值)
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	//U方向上的寻址模式为CLAMP（钳位寻址模式）
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	//V方向上的寻址模式为CLAMP（钳位寻址模式）
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);	//W方向上的寻址模式为CLAMP（钳位寻址模式）

	//过滤器LINEAR,寻址模式WRAP的静态采样器
	CD3DX12_STATIC_SAMPLER_DESC linearWarp(2,	//着色器寄存器
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,		//过滤器类型为LINEAR(线性插值)
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,	//U方向上的寻址模式为WRAP（重复寻址模式）
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,	//V方向上的寻址模式为WRAP（重复寻址模式）
		D3D12_TEXTURE_ADDRESS_MODE_WRAP);	//W方向上的寻址模式为WRAP（重复寻址模式）

	//过滤器LINEAR,寻址模式CLAMP的静态采样器
	CD3DX12_STATIC_SAMPLER_DESC linearClamp(3,	//着色器寄存器
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,		//过滤器类型为LINEAR(线性插值)
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	//U方向上的寻址模式为CLAMP（钳位寻址模式）
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	//V方向上的寻址模式为CLAMP（钳位寻址模式）
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);	//W方向上的寻址模式为CLAMP（钳位寻址模式）

	//过滤器ANISOTROPIC,寻址模式WRAP的静态采样器
	CD3DX12_STATIC_SAMPLER_DESC anisotropicWarp(4,	//着色器寄存器
		D3D12_FILTER_ANISOTROPIC,			//过滤器类型为ANISOTROPIC(各向异性)
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,	//U方向上的寻址模式为WRAP（重复寻址模式）
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,	//V方向上的寻址模式为WRAP（重复寻址模式）
		D3D12_TEXTURE_ADDRESS_MODE_WRAP);	//W方向上的寻址模式为WRAP（重复寻址模式）

	//过滤器LINEAR,寻址模式CLAMP的静态采样器
	CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(5,	//着色器寄存器
		D3D12_FILTER_ANISOTROPIC,			//过滤器类型为ANISOTROPIC(各向异性)
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	//U方向上的寻址模式为CLAMP（钳位寻址模式）
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	//V方向上的寻址模式为CLAMP（钳位寻址模式）
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);	//W方向上的寻址模式为CLAMP（钳位寻址模式）

	return{ pointWarp, pointClamp, linearWarp, linearClamp, anisotropicWarp, anisotropicClamp };
}

void DXWindow::LoadAssets()
{
    // 1.
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
            assert(false && "Root Signature Version error");
        }

        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
        
        CD3DX12_DESCRIPTOR_RANGE1 ranges[1] = {};
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
        // TODO: sampler

        // A single 32-bit constant root parameter that is used by the vertex shader.
        CD3DX12_ROOT_PARAMETER1 rootParameters[3] = {};
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[1].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);
        rootParameters[2].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);
        
        // D3D12_STATIC_SAMPLER_DESC samplerDescs[1] = {};
        auto samplerDescs = GetStaticSamplers();

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(
            _countof(rootParameters),
            rootParameters,
            samplerDescs.size(),
            samplerDescs.data(),
            rootSignatureFlags);
        // rootSignatureDesc.Init(0, nullptr, 0, nullptr, rootSignatureFlags);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        // ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error));
        ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error));
        ThrowIfFailed(m_device->CreateRootSignature(
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
            GetShaderFullPath(L"shaders.hlsl").c_str(),
            nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_1",
            compileFlags, 0, &vertexShader, nullptr
            )
        );
        ThrowIfFailed(D3DCompileFromFile(
            GetShaderFullPath(L"shaders.hlsl").c_str(),
            nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_1",
            compileFlags, 0, &pixelShader, nullptr
            )
        );

        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        struct PipelineStateStream
        {
            CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
            CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
            CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
            CD3DX12_PIPELINE_STATE_STREAM_VS VS;
            CD3DX12_PIPELINE_STATE_STREAM_PS PS;
            CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
            CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
        } pipelineStateStream;

        D3D12_RT_FORMAT_ARRAY rtvFormats = {};
        rtvFormats.NumRenderTargets = 1;
        rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

        pipelineStateStream.pRootSignature = m_RootSignature.Get();
        pipelineStateStream.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        pipelineStateStream.RTVFormats = rtvFormats;

        D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {};
        pipelineStateStreamDesc.SizeInBytes = sizeof(PipelineStateStream);
        pipelineStateStreamDesc.pPipelineStateSubobjectStream = &pipelineStateStream;
        ThrowIfFailed(m_device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_PipelineState)));
    }

    auto commandList = m_commandQueue->GetCommandList(nullptr);
    
    ComPtr<ID3D12Resource> intermediateVertexBuffer;
    ComPtr<ID3D12Resource> intermediateIndexBuffer;

    // 4.
    {
        m_model = Application::GetInstance()->GetModel();
        auto vertices = m_model->GetVertices();
        auto numVertices = m_model->GetVerticesNum();
        auto indicies = m_model->GetIndicies();
        auto numIndicies = m_model->GetIndiciesNum();


        // Upload vertex buffer data.
        UpdateBufferResource(commandList, &m_VertexBuffer, &intermediateVertexBuffer,
            numVertices, sizeof(Vertex), vertices.data());

        // Create the vertex buffer view.
        m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
        m_VertexBufferView.SizeInBytes = numVertices * sizeof(Vertex);
        m_VertexBufferView.StrideInBytes = sizeof(Vertex);

        // Upload index buffer data.
        UpdateBufferResource(commandList, &m_IndexBuffer, &intermediateIndexBuffer,
            numIndicies, sizeof(uint32_t), indicies.data());

        // Create index buffer view.
        m_IndexBufferView.BufferLocation = m_IndexBuffer->GetGPUVirtualAddress();
        m_IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
        m_IndexBufferView.SizeInBytes = numIndicies * sizeof(uint32_t);
    }

    // constant upload buffer
    {
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(g_mvpSize*2),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_mvpUploadBuffer)));
        ThrowIfFailed(m_mvpUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&g_gpuMVPCB)));

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc1 = {};
        cbvDesc1.BufferLocation = m_mvpUploadBuffer->GetGPUVirtualAddress();
        cbvDesc1.SizeInBytes = g_mvpSize;
        m_device->CreateConstantBufferView(&cbvDesc1, m_CBVDescriptorHeap->GetCPUHeapStartPtr());

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(g_passDataSize*2),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_passDataUploadBuffer)
        ));
        ThrowIfFailed(m_passDataUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&g_gpuPassData)));

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc2 = {};
        cbvDesc2.BufferLocation = m_passDataUploadBuffer->GetGPUVirtualAddress();
        cbvDesc2.SizeInBytes = g_passDataSize;
        m_device->CreateConstantBufferView(&cbvDesc2, m_CBVDescriptorHeap->GetCPUHeapStartPtr(1));
    }

    // 2D texture
    ComPtr<ID3D12Resource> intermediateTextureBuffer;
    {
        ComPtr<IWICImagingFactory> pIWICFactory;
        ThrowIfFailed(CoCreateInstance(
            CLSID_WICImagingFactory, 
            nullptr, 
            CLSCTX_INPROC_SERVER, 
            IID_PPV_ARGS(&pIWICFactory)));
        
        ComPtr<IWICBitmapDecoder> pIWICDecoder;
        ThrowIfFailed(pIWICFactory->CreateDecoderFromFilename(
            GetAssetFullPath(L"model/african_head_diffuse.jpg").c_str(),
            // GetAssetFullPath(L"model/bear.jpg").c_str(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnDemand,
            &pIWICDecoder
        ));

        ComPtr<IWICBitmapFrameDecode> pIWICFrameDecoder;
        ThrowIfFailed(pIWICDecoder->GetFrame(0, &pIWICFrameDecoder));

        WICPixelFormatGUID wpf = {};
        ThrowIfFailed(pIWICFrameDecoder->GetPixelFormat(&wpf));

        GUID tgFormat = {};
        DXGI_FORMAT textureFormat = DXGI_FORMAT_UNKNOWN;
        if (GetTargetPixelFormat(&wpf, &tgFormat))
        {
            textureFormat = GetDXGIFormatFromPixelFormat(&tgFormat);
        }
        assert(textureFormat != DXGI_FORMAT_UNKNOWN && "texture format error.");

        ComPtr<IWICBitmapSource> pIWICSource;
        if (!InlineIsEqualGUID(wpf, tgFormat))
        {
            ComPtr<IWICFormatConverter> formatConverter;
            ThrowIfFailed(pIWICFactory->CreateFormatConverter(&formatConverter));

            ThrowIfFailed(formatConverter->Initialize(
                pIWICFrameDecoder.Get(),
                tgFormat,
                WICBitmapDitherTypeNone,
                nullptr,
                0.f,
                WICBitmapPaletteTypeCustom
            ));
            ThrowIfFailed(formatConverter.As(&pIWICSource));
        }
        else
        {
            ThrowIfFailed(pIWICFrameDecoder.As(&pIWICSource));
        }

        UINT textureW;
        UINT textureH;
        ThrowIfFailed(pIWICSource->GetSize(&textureW, &textureH));

        ComPtr<IWICComponentInfo> componentInfo;
        ThrowIfFailed(pIWICFactory->CreateComponentInfo(tgFormat, componentInfo.GetAddressOf()));

        WICComponentType type;
        ThrowIfFailed(componentInfo->GetComponentType(&type));
        assert(type == WICPixelFormat && "WICPixelFormat error.");

        ComPtr<IWICPixelFormatInfo> pixelInfo;
        ThrowIfFailed(componentInfo.As(&pixelInfo));
        UINT bitsPerPixel = 0u;
        ThrowIfFailed(pixelInfo->GetBitsPerPixel(&bitsPerPixel));

        double bits = textureW * bitsPerPixel;
        UINT rowPitch = std::ceil(bits / 8.); // the num of bytes

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Tex2D(textureFormat, textureW, textureH),
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_texture)
        ));

        UINT64 bufferSize = 0;
        auto textureDesc = m_texture->GetDesc();
        m_device->GetCopyableFootprints(
            &m_texture->GetDesc(), 0, 1, 0, nullptr, nullptr, nullptr,
            &bufferSize
        );

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&intermediateTextureBuffer)
        ));

        BYTE* bufferData = new BYTE[rowPitch * textureH];
        ThrowIfFailed(pIWICSource->CopyPixels(
            nullptr,
            rowPitch,
            rowPitch * textureH,
            reinterpret_cast<BYTE*>(bufferData)
        ));

        D3D12_SUBRESOURCE_DATA subresourceData = {};
        subresourceData.pData = bufferData;
        subresourceData.RowPitch = rowPitch;
        subresourceData.SlicePitch = subresourceData.RowPitch;

        UpdateSubresources(commandList.Get(),
            m_texture.Get(), intermediateTextureBuffer.Get(),
            0, 0, 1, &subresourceData);

        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_texture.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList->ResourceBarrier(1, &barrier);

        m_textureView = {};
        m_textureView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        m_textureView.Format = textureDesc.Format;
        m_textureView.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        m_textureView.Texture2D.MipLevels = 1;
        m_device->CreateShaderResourceView(m_texture.Get(), &m_textureView, m_SRVDescriptorHeap->GetCPUHeapStartPtr());
    }
    
    // shadow map
    {
        // shadow map PSO
        {
            ComPtr<ID3DBlob> vertexShader;
            ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
            UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
            UINT compileFlags = 0;
#endif

            ThrowIfFailed(D3DCompileFromFile(
                GetShaderFullPath(L"shadow.hlsl").c_str(),
                nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_1",
                compileFlags, 0, &vertexShader, nullptr
            ));
            ThrowIfFailed(D3DCompileFromFile(
                GetShaderFullPath(L"shadow.hlsl").c_str(),
                nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_1",
                compileFlags, 0, &pixelShader, nullptr
            ));
            

            D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
            };
            
            struct PipelineStateStream
            {
                CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
                CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
                CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
                CD3DX12_PIPELINE_STATE_STREAM_VS VS;
                CD3DX12_PIPELINE_STATE_STREAM_PS PS;
                CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
                CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
                CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
            };

            D3D12_RT_FORMAT_ARRAY rtvFormats = {};
            
            PipelineStateStream shadowPipelineStateStream;
            shadowPipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
            rtvFormats.NumRenderTargets = 0;
            rtvFormats.RTFormats[0] = DXGI_FORMAT_UNKNOWN;
            shadowPipelineStateStream.RTVFormats = rtvFormats;
            shadowPipelineStateStream.pRootSignature = m_RootSignature.Get();
            shadowPipelineStateStream.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
            shadowPipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            shadowPipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
            shadowPipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
            CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
            rasterizerDesc.DepthBias = 0;
            rasterizerDesc.DepthBiasClamp = 0;
            rasterizerDesc.SlopeScaledDepthBias = 1.f;
            shadowPipelineStateStream.Rasterizer = CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER(rasterizerDesc);

            D3D12_PIPELINE_STATE_STREAM_DESC shadowPipelineStateStreamDesc = {};
            shadowPipelineStateStreamDesc.pPipelineStateSubobjectStream = &shadowPipelineStateStream;
            shadowPipelineStateStreamDesc.SizeInBytes = sizeof(PipelineStateStream);
            ThrowIfFailed(m_device->CreatePipelineState(&shadowPipelineStateStreamDesc, IID_PPV_ARGS(&m_shadowPipelineState)));
        }

        // shadow map resource
        {
            m_shadowMapH = 2048;
            m_shadowMapW = 2048;

            // CD3DX12_RESOURCE_DESC resourceDesc = {};
            // resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            // resourceDesc.Alignment = 0;
            // resourceDesc.Width = m_shadowMapW;
            // resourceDesc.Height = m_shadowMapH;
            // resourceDesc.DepthOrArraySize = 1;
            // resourceDesc.MipLevels = 1;
            // resourceDesc.Format = DXGI_FORMAT_D32_FLOAT;
            // resourceDesc.SampleDesc.Count = 1;
            // resourceDesc.SampleDesc.Quality = 0;
            // resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            // resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            CD3DX12_CLEAR_VALUE optClear(DXGI_FORMAT_D32_FLOAT, 1.0, 0);

            ThrowIfFailed(m_device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_shadowMapW, m_shadowMapH,
                    1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                &optClear,
                IID_PPV_ARGS(&m_shadowMap)
            ));
        }
        // shadow map dsv/srv
        {
            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = 0;
            dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
            m_device->CreateDepthStencilView(m_shadowMap.Get(), &dsvDesc, m_DSVDescriptorHeap->GetCPUHeapStartPtr(1));

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.PlaneSlice = 0;
            srvDesc.Texture2D.ResourceMinLODClamp = 0;
            m_device->CreateShaderResourceView(m_shadowMap.Get(), &srvDesc, m_SRVDescriptorHeap->GetCPUHeapStartPtr(1));
        }

    }

    ComPtr<ID3D12Resource> debugIntermediateVertexBuffer;
    ComPtr<ID3D12Resource> debugIntermediateIndexBuffer;
    // shadow debug
    {
        // PSO
        {
            ComPtr<ID3DBlob> vertexShader;
            ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
            UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
            UINT compileFlags = 0;
#endif

            ThrowIfFailed(D3DCompileFromFile(
                GetShaderFullPath(L"shadowDebug.hlsl").c_str(),
                nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_1",
                compileFlags, 0, &vertexShader, nullptr
            ));
            ThrowIfFailed(D3DCompileFromFile(
                GetShaderFullPath(L"shadowDebug.hlsl").c_str(),
                nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_1",
                compileFlags, 0, &pixelShader, nullptr
            ));
            

            D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
            };
            
            struct PipelineStateStream
            {
                CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
                CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
                CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
                CD3DX12_PIPELINE_STATE_STREAM_VS VS;
                CD3DX12_PIPELINE_STATE_STREAM_PS PS;
                CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
                CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
            };

            D3D12_RT_FORMAT_ARRAY rtvFormats = {};
            
            PipelineStateStream debugPSO;
            debugPSO.DSVFormat = DXGI_FORMAT_D32_FLOAT;
            rtvFormats.NumRenderTargets = 1;
            rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
            debugPSO.RTVFormats = rtvFormats;
            debugPSO.pRootSignature = m_RootSignature.Get();
            debugPSO.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
            debugPSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            debugPSO.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
            debugPSO.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());

            D3D12_PIPELINE_STATE_STREAM_DESC debugPSOStreamDesc = {};
            debugPSOStreamDesc.pPipelineStateSubobjectStream = &debugPSO;
            debugPSOStreamDesc.SizeInBytes = sizeof(PipelineStateStream);
            ThrowIfFailed(m_device->CreatePipelineState(&debugPSOStreamDesc, IID_PPV_ARGS(&m_shadowDebugPipelineState)));
        }
        
        {
            Vertex vertices[] = {
                Vertex{XMFLOAT3(0.5, -0.9, 0), XMFLOAT3(0, 0, 1), XMFLOAT2(0, 1)},
                Vertex{XMFLOAT3(0.5, -0.5, 0), XMFLOAT3(0, 0, 1), XMFLOAT2(0, 0)},
                Vertex{XMFLOAT3(0.9, -0.5, 0), XMFLOAT3(0, 0, 1), XMFLOAT2(1, 0)},
                Vertex{XMFLOAT3(0.9, -0.9, 0), XMFLOAT3(0, 0, 1), XMFLOAT2(1, 1)}
            };

            uint32_t indicies[] = { 0, 1, 2,  0, 2, 3};
            // uint32_t indicies[] = { 0, 2, 1,  0, 3, 2};
            
            UpdateBufferResource(commandList, &m_debugRectVertexBuffer, &debugIntermediateVertexBuffer,
                _countof(vertices), sizeof(Vertex), vertices);

            // Create the vertex buffer view.
            m_debugRectVertexBufferView.BufferLocation = m_debugRectVertexBuffer->GetGPUVirtualAddress();
            m_debugRectVertexBufferView.SizeInBytes = _countof(vertices) * sizeof(Vertex);
            m_debugRectVertexBufferView.StrideInBytes = sizeof(Vertex);

            // Upload index buffer data.
            UpdateBufferResource(commandList, &m_debugRectIndexBuffer, &debugIntermediateIndexBuffer,
                _countof(indicies), sizeof(uint32_t), indicies);

            // Create index buffer view.
            m_debugRectIndexBufferView.BufferLocation = m_debugRectIndexBuffer->GetGPUVirtualAddress();
            m_debugRectIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
            m_debugRectIndexBufferView.SizeInBytes = _countof(indicies) * sizeof(uint32_t);
        }
    }
    m_commandQueue->ExecuteCommandList(commandList);

    ResizeDepthBuffer(m_width, m_height);
}

void DXWindow::Init(HWND hWnd)
{
    m_hWnd = hWnd;
    
    // Initialize the global window rect variable.
    ::GetWindowRect(m_hWnd, &m_windowRect);

    LoadPipeline();
    LoadAssets();
    
    m_isInitialized = true;
}

void DXWindow::Destroy()
{
    m_commandQueue->Destory();
    m_device->Release();

    m_swapChain.reset();
    m_commandQueue.reset();
    m_RTVDescriptorHeap.reset();
    m_DSVDescriptorHeap.reset();
    m_SRVDescriptorHeap.reset();
    m_CBVDescriptorHeap.reset();

    m_RootSignature->Release();
    m_PipelineState->Release();
    m_VertexBuffer->Release();
    m_IndexBuffer->Release();
    m_DepthBuffer->Release();

    m_texture->Release();

    m_mvpUploadBuffer->Unmap(0, nullptr);
    m_passDataUploadBuffer->Unmap(0, nullptr);
    m_mvpUploadBuffer->Release();
    m_passDataUploadBuffer->Release();

    m_shadowPipelineState->Release();
    m_shadowDebugPipelineState->Release();

    m_shadowMap->Release();

    m_debugRectVertexBuffer->Release();
    m_debugRectIndexBuffer->Release();
}

HWND DXWindow::GetHandler() const
{
    return m_hWnd;
}
uint32_t DXWindow::GetWidth() const
{
    return m_width;
}
uint32_t DXWindow::GetHeight() const
{
    return m_height;
}
const wchar_t* DXWindow::GetName() const
{
    return m_name;
}
bool DXWindow::IsInitialized() const
{
    return m_isInitialized;
}
void DXWindow::SetVSync(bool VSync)
{
    m_swapChain->SetVSync(VSync);
}
bool DXWindow::IsVSync() const
{
    return m_swapChain->IsVSync();
}
bool DXWindow::IsFullscreen() const
{
    return m_fullscreen;
}

void DXWindow::Update()
{
    static uint64_t frameCounter = 0;
    static double elapsedSeconds = 0.0;
    static std::chrono::high_resolution_clock clock;
    static auto t0 = clock.now();
    static double totalTime = 0.;

    frameCounter++;
    auto t1 = clock.now();
    auto deltaTime = t1 - t0;
    totalTime += deltaTime.count() * 1e-9;
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

    // Update the model matrix.
    // float angle = static_cast<float>(totalTime * 90.0);
    float angle = 0.f;
    const XMVECTOR rotationAxis = XMVectorSet(0, 1, 0, 0);
    // auto scale = XMMatrixScaling(30.f, 30.f, 30.f);
    auto scale = XMMatrixScaling(1.f, 1.f, 1.f);
    auto rotation = XMMatrixRotationAxis(rotationAxis, XMConvertToRadians(angle));
    // auto translation = XMMatrixTranslation(0.f, -3.f, 0.f);
    auto translation = XMMatrixTranslation(0.f, 0.f, 0.f);
    
    // DXMath 里，变换是行向量左乘矩阵
    // m_ModelMatrix = XMMatrixMultiply(XMMatrixMultiply(scale, rotation), translation); // C-style
    m_ModelMatrix = scale * rotation * translation;

    // Update the MVP matrix
    // XMMATRIX mvpMatrix = XMMatrixMultiply(m_ModelMatrix, m_ViewMatrix);
    // mvpMatrix = XMMatrixMultiply(mvpMatrix, m_ProjectionMatrix); // C-style
    // DXMath中矩阵是行主序，hlsl中是列主序，在C++层面做一层转置效率更高
    auto mvp = m_ModelMatrix * m_camera->GetViewMatrix() * m_camera->GetProjectionMatrix();
    g_MVPCB.mvp = XMMatrixTranspose(mvp);
    // mvp.r[3] = XMVectorSet(0.f, 0.f, 0.f, 1.f);
    g_MVPCB.modelMatrixNegaTrans = XMMatrixInverse(nullptr, m_ModelMatrix);
    g_MVPCB.modelMatrix = XMMatrixTranspose(m_ModelMatrix);

    g_passData.eyePos = m_camera->GetPosition();
    angle = static_cast<float>(totalTime);
    g_passData.lightPos = XMFLOAT4(-3.f * std::sin(angle), 3.f, 3.f * std::cos(angle), 1.f);
    // g_passData.lightPos = XMFLOAT4(5.f, 2.f, 0.f, 1.f);
    // g_passData.lightPos = XMFLOAT4(0.f, 0.f, -2.f, 1.f);
}

inline XMMATRIX ShadowData()
{
    // 主光才投射物体阴影
	// XMVECTOR lightDir = XMLoadFloat3(&mRotatedLightDir[0]);
	XMVECTOR lightPos = XMLoadFloat4(&g_passData.lightPos);
	XMVECTOR targetPos = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	// WorldToLight矩阵 （世界空间转灯光空间）
	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

	// XMStoreFloat3(&mLightPosW, lightPos);//灯光坐标

	// 将包围球变换到光源空间
	XMFLOAT3 sphereCenterLS;
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

	// 位于光源空间中包围场景的正交投影视景体
	float l = sphereCenterLS.x - 3;//左端点
	float b = sphereCenterLS.y - 3;//下端点
	float n = sphereCenterLS.z - 3;//近端点
	float r = sphereCenterLS.x + 3;//右端点
	float t = sphereCenterLS.y + 3;//上端点
	float f = sphereCenterLS.z + 3;//远端点

	//构建LightToProject矩阵（灯光空间转NDC空间）
	XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	// 构建NDCToTexture矩阵（NDC空间转纹理空间）
	// 从[-1, 1]转到[0, 1]
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f,-0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	// 构建LightToTexture（灯光空间转纹理空间）
	XMMATRIX S = lightView * lightProj;
    g_passData.lightVp = XMMatrixTranspose(S * T);
	// XMStoreFloat4x4(&mLightView, lightView);
	// XMStoreFloat4x4(&mLightProj, lightProj);
	// XMStoreFloat4x4(&mShadowTransform, S);
    return S;
}

inline void DXWindow::UpdateShadowPassData(ComPtr<ID3D12GraphicsCommandList2>& commandList)
{
    // TODO: 现在是平行光照，从pos照向原点
    Camera* lightView = new OrthographicCamera(m_shadowMapW, m_shadowMapH, 0.1f, 100.f, g_passData.lightPos);
    MVPData lightMVP;
    auto mvp = ShadowData();
    // auto mvp = lightView->GetViewMatrix() * lightView->GetProjectionMatrix();
    lightMVP.mvp = XMMatrixTranspose(mvp);
    lightMVP.modelMatrixNegaTrans = XMMatrixInverse(nullptr, m_ModelMatrix);
    lightMVP.modelMatrix = XMMatrixTranspose(m_ModelMatrix);

    BYTE* gpuMVPCB = g_gpuMVPCB + g_mvpSize;
    BYTE* gpuPassData = g_gpuPassData + g_passDataSize;
    memcpy(gpuMVPCB, &lightMVP, sizeof(lightMVP));
    memcpy(gpuPassData, &g_passData, sizeof(g_passData));

    commandList->SetGraphicsRootConstantBufferView(1, m_mvpUploadBuffer->GetGPUVirtualAddress() + g_mvpSize);
    commandList->SetGraphicsRootConstantBufferView(2, m_passDataUploadBuffer->GetGPUVirtualAddress() + g_passDataSize);
    
    delete lightView;
}

void DXWindow::ShadowPass(ComPtr<ID3D12GraphicsCommandList2>& commandList)
{
    commandList->SetPipelineState(m_shadowPipelineState.Get());
    auto viewport = CD3DX12_VIEWPORT(0.f, 0, m_shadowMapW, m_shadowMapH);
    auto scissorRect = CD3DX12_RECT(0, 0, m_shadowMapW, m_shadowMapH);
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    UpdateShadowPassData(commandList);

    commandList->ResourceBarrier(1,
        &CD3DX12_RESOURCE_BARRIER::Transition(m_shadowMap.Get(),
            D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));
    commandList->ClearDepthStencilView(m_DSVDescriptorHeap->GetCPUHeapStartPtr(1),
        D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    commandList->OMSetRenderTargets(0, nullptr, FALSE, &m_DSVDescriptorHeap->GetCPUHeapStartPtr(1));
    
    // Set obj
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
    commandList->IASetIndexBuffer(&m_IndexBufferView);
    commandList->DrawIndexedInstanced(m_model->GetIndiciesNum(), 1, 0, 0, 0);

    commandList->ResourceBarrier(1,
        &CD3DX12_RESOURCE_BARRIER::Transition(m_shadowMap.Get(),
            D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void DXWindow::Render()
{
    auto commandList = m_commandQueue->GetCommandList(nullptr);
    
    // Set necessary state.
    commandList->SetGraphicsRootSignature(m_RootSignature.Get());

    ID3D12DescriptorHeap* ppHeaps[] = { m_SRVDescriptorHeap->GetHeap().Get() };
    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    //commandList->SetGraphicsRootDescriptorTable(0, m_SRVDescriptorHeap->GetGPUHeapStartPtr(0));
    
    ShadowPass(commandList);
    commandList->SetGraphicsRootDescriptorTable(0, m_SRVDescriptorHeap->GetGPUHeapStartPtr(0));

    commandList->SetPipelineState(m_PipelineState.Get());
    commandList->RSSetViewports(1, &m_viewport);
    commandList->RSSetScissorRects(1, &m_scissorRect);

    //commandList->SetGraphicsRootDescriptorTable(0, m_SRVDescriptorHeap->GetGPUHeapStartPtr(0));

    auto RTVHandle = m_RTVDescriptorHeap->GetCPUHeapStartPtr(m_swapChain->GetCurrentBackBufferIndex());
    auto DSVHandle = m_DSVDescriptorHeap->GetCPUHeapStartPtr();
    m_swapChain->ClearRenderTarget(commandList, RTVHandle, DSVHandle);

    memcpy(g_gpuMVPCB, &g_MVPCB, sizeof(g_MVPCB));
    memcpy(g_gpuPassData, &g_passData, sizeof(g_passData));

    commandList->SetGraphicsRootConstantBufferView(1, m_mvpUploadBuffer->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(2, m_passDataUploadBuffer->GetGPUVirtualAddress());
    
    // Set obj
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
    commandList->IASetIndexBuffer(&m_IndexBufferView);
    commandList->DrawIndexedInstanced(m_model->GetIndiciesNum(), 1, 0, 0, 0);

    // shadow debug
    commandList->SetPipelineState(m_shadowDebugPipelineState.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &m_debugRectVertexBufferView);
    commandList->IASetIndexBuffer(&m_debugRectIndexBufferView);
    commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

    m_swapChain->Present(commandList);
}

void DXWindow::UpdateWindowRect(uint32_t width, uint32_t height)
{
    m_width = std::max(1u, width);
    m_height = std::max(1u, height);

    m_swapChain->Resize(m_width, m_height, m_RTVDescriptorHeap);
    m_viewport = CD3DX12_VIEWPORT(0.f, 0.f,
        static_cast<float>(m_width), static_cast<float>(m_height));
    m_scissorRect = CD3DX12_RECT(0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height));
    m_camera->UpdateViewport(static_cast<float>(m_width), static_cast<float>(m_height));

}

void DXWindow::SetFullscreen(bool fullscreen)
{
    if (m_fullscreen != fullscreen)
    {
        m_fullscreen = fullscreen;
        uint32_t width = 1;
        uint32_t height = 1;

        if (m_fullscreen) // Switching to fullscreen.
        {
            // Store the current window dimensions so they can be restored 
            // when switching out of fullscreen state.
            ::GetWindowRect(m_hWnd, &m_windowRect);
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

            width = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
            height = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
        }
        else
        {
            // Restore all the window decorators.
            ::SetWindowLongW(m_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

            ::SetWindowPos(m_hWnd, HWND_NOTOPMOST,
                m_windowRect.left,
                m_windowRect.top,
                m_windowRect.right - m_windowRect.left,
                m_windowRect.bottom - m_windowRect.top,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);

            ::ShowWindow(m_hWnd, SW_NORMAL);

            width = m_windowRect.right - m_windowRect.left;
            height = m_windowRect.bottom - m_windowRect.top;
        }
        
        UpdateWindowRect(width, height);
        ResizeDepthBuffer(m_width, m_height);
    }
}

void DXWindow::Resize(uint32_t width, uint32_t height)
{
    if (m_width != width || m_height != height)
    {
        UpdateWindowRect(width, height);
        ResizeDepthBuffer(m_width, m_height);
    }
}

void DXWindow::ResizeDepthBuffer(uint32_t width, uint32_t height)
{
    // Flush any GPU commands that might be referencing the depth buffer.
    m_commandQueue->Flush();

    width = std::max(1u, width);
    height = std::max(1u, height);

    // Resize screen dependent resources.
    // Create a depth buffer.
    D3D12_CLEAR_VALUE optimizedClearValue = {};
    optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    optimizedClearValue.DepthStencil = { 1.0f, 0 };

    ThrowIfFailed(m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height,
            1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &optimizedClearValue,
        IID_PPV_ARGS(&m_DepthBuffer)
    ));

    // Update the depth-stencil view.
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
    dsv.Format = DXGI_FORMAT_D32_FLOAT;
    dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv.Texture2D.MipSlice = 0;
    dsv.Flags = D3D12_DSV_FLAG_NONE;

    m_device->CreateDepthStencilView(m_DepthBuffer.Get(), &dsv,
        m_DSVDescriptorHeap->GetCPUHeapStartPtr());
}

LRESULT DXWindow::OnWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (IsInitialized())
    {
        switch (message)
        {
        case WM_PAINT:
            Update();
            Render();
            break;
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        {
            bool alt = (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

            switch (wParam)
            {
            case 'V':
                SetVSync(!IsVSync());
                break;
            case VK_ESCAPE:
                ::PostQuitMessage(0);
                break;
            case VK_RETURN:
                if ( alt )
                {
            case VK_F11:
                SetFullscreen(!IsFullscreen());
                }
                break;
            }
        }
        break;
        // The default window procedure will play a system notification sound 
        // when pressing the Alt+Enter keyboard combination if this message is 
        // not handled.
        case WM_SYSCHAR:
        break;
        case WM_SIZE:
        {
            RECT clientRect = {};
            ::GetClientRect(hWnd, &clientRect);

            int width = clientRect.right - clientRect.left;
            int height = clientRect.bottom - clientRect.top;

            Resize(width, height);
        }
        break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            break;
        default:
            return ::DefWindowProcW(hWnd, message, wParam, lParam);
        }
    }
    else
    {
        return ::DefWindowProcW(hWnd, message, wParam, lParam);
    }

    return 0;
}