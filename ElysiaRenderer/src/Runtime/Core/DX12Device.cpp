#include "stdafx.h"
#include "DX12Device.h"

#include "ThirdParty/D3D12MemoryAllocator/D3D12MemAlloc.h"
#include "DX12RenderPassDescriptorHeap.h"
#include "DX12GraphicsContext.h"
#include "DX12UploadContext.h"
#include "DX12BufferResource.h"
#include "DX12TextureBuffer.h"
#include "DX12Context.h"
#include "DX12StagingDescriptorHeap.h"
#include "DX12Queue.h"
#include "DX12Shader.h"
#include "DX12RootSignature.h"
#include "DX12PipelineState.h"
#include "ShaderKeywordSpace.h"

#include "Runtime/RenderCore/RenderResource.h"
#include "UploadRingBuffer.h"
#include "Programs/UserMarker.h"
#include "Runtime/RenderCore/BufferManager.h"
#include "Runtime/Core/ShaderCompileOptions.h"
#include "Runtime/RenderCore/ShaderVariantManager.h"
#include "ThirdParty/Misc.h"
#include "lib/AMD/libs/AGS/amd_ags.h"


namespace ElysiaCore
{
    using namespace ElysiaHelper;

    DX12Device::DX12Device() = default;
    DX12Device::~DX12Device()
    {
        WaitForIdle();

        for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
        {
            ProcessDestruction(i);
        }
        m_RTVStagingDescriptorHeap = nullptr;
        m_DSVStagingDescriptorHeap = nullptr;
        m_samplerRenderPassDescriptorHeap = nullptr;

        for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
        {
            m_SRVRenderPassDescriptorHeaps[i] = nullptr;
            m_uploadContexts[i] = nullptr;
        }

        m_graphicsQueue = nullptr;
        m_computeQueue = nullptr;
        m_copyQueue = nullptr;

#ifdef DEBUG
        IDXGIDebug1* pDebug = nullptr;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug))))
        {
            pDebug->ReportLiveObjects(DXGI_DEBUG_ALL,
                                      DXGI_DEBUG_RLO_FLAGS(
                                          DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_DETAIL |
                                          DXGI_DEBUG_RLO_IGNORE_INTERNAL));
            ElysiaHelper::SafeRelease(pDebug);
        }
#endif
    }

    void DX12Device::OnCreate(std::wstring appName, bool bCPUValidationEnabled,
                              bool bGpuValidationEnabled)
    {
#if defined(_DEBUG)
        CComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
        }
#endif

        // Enable the D3D12 debug layer
        //
        // Note that it turns out the validation and debug layer are known to cause
        // deadlocks in certain circumstances, for example when the vsync interval
        // is 0 and full screen is used
        if (bCPUValidationEnabled || bGpuValidationEnabled)
        {
            ID3D12Debug1* pDebugController;
            if (D3D12GetDebugInterface(IID_PPV_ARGS(&pDebugController)) == S_OK)
            {
                // Enabling GPU Validation without enabling the debug layer does nothing
                if (bCPUValidationEnabled || bGpuValidationEnabled)
                {
                    // pDebugController->EnableDebugLayer();
                    // pDebugController->SetEnableGPUBasedValidation(bGpuValidationEnabled);
                }
                pDebugController->Release();
            }
        }

        // Initialize Adapter
        {
            UINT factoryFlags = 0;
            if (bCPUValidationEnabled || bGpuValidationEnabled)
                factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

            IDXGIFactory* pFactory;
            ThrowIfFailed(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&pFactory)));
            ThrowIfFailed(pFactory->EnumAdapters(0, &m_pAdapter));
            pFactory->Release();
        }

        // Create Device
        //
        {
            // check if AMD GPU
            DXGI_ADAPTER_DESC AdapterDesc;
            m_pAdapter->GetDesc(&AdapterDesc);
            const bool bAMDGPU = (AdapterDesc.VendorId == 0x1002);

            if (bAMDGPU)
            {
                AGSReturnCode result = agsInitialize(
                    AGS_MAKE_VERSION(AMD_AGS_VERSION_MAJOR, AMD_AGS_VERSION_MINOR,
                                     AMD_AGS_VERSION_PATCH), nullptr, &m_agsContext, &m_agsGPUInfo);
                if (result == AGS_SUCCESS)
                {
                    UserMarker::SetAgsContext(m_agsContext);

                    AGSDX12DeviceCreationParams creationParams = {};
                    creationParams.pAdapter = m_pAdapter;
                    creationParams.iid = __uuidof(m_pDevice);
                    creationParams.FeatureLevel = D3D_FEATURE_LEVEL_12_0;

                    AGSDX12ExtensionParams extensionParams = {};
                    AGSDX12ReturnedParams returnedParams = {};

                    // Create AGS Device
                    //
                    AGSReturnCode rc = agsDriverExtensionsDX12_CreateDevice(
                        m_agsContext, &creationParams, &extensionParams, &returnedParams);
                    if (rc == AGS_SUCCESS)
                    {
                        m_pDevice = returnedParams.pDevice;
                    }
                    else
                    {
                        Trace("Warning: AGS CreateDevice() failed w/ code=%d", rc);
                    }
                }
                else
                {
                    Trace("Warning: agsInitialize() failed w/ code=%d", result);
                }
            }
        }

        if (!m_pDevice)
        {
            ThrowIfFailed(D3D12CreateDevice(m_pAdapter, D3D_FEATURE_LEVEL_12_0,
                                            IID_PPV_ARGS(&m_pDevice)));

            if (bCPUValidationEnabled || bGpuValidationEnabled)
            {
                ID3D12InfoQueue* pInfoQueue;
                if (m_pDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue)) == S_OK)
                {
                    pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
                    pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
                }
            }
        }
        SetName(m_pDevice, "device");

        // Check for FP16 support
        D3D12_FEATURE_DATA_D3D12_OPTIONS featureDataOptions = {};
        ThrowIfFailed(m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,
                                                     &featureDataOptions,
                                                     sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS)));
        m_fp16Supported = (featureDataOptions.MinPrecisionSupport &
                           D3D12_SHADER_MIN_PRECISION_SUPPORT_16_BIT) != 0;

        // Check for RT support
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureDataOptions5 = {};
        ThrowIfFailed(m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,
                                                     &featureDataOptions5,
                                                     sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5)));
        m_rt10Supported = (featureDataOptions5.RaytracingTier & D3D12_RAYTRACING_TIER_1_0) != 0;
        m_rt11Supported = (featureDataOptions5.RaytracingTier & D3D12_RAYTRACING_TIER_1_1) != 0;

        // Check for VariableShadingRate support
        D3D12_FEATURE_DATA_D3D12_OPTIONS6 featureDataOptions6 = {};
        ThrowIfFailed(m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6,
                                                     &featureDataOptions6,
                                                     sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS6)));
        m_vrs1Supported = (featureDataOptions6.VariableShadingRateTier &
                           D3D12_VARIABLE_SHADING_RATE_TIER_1) != 0;
        m_vrs2Supported = (featureDataOptions6.VariableShadingRateTier &
                           D3D12_VARIABLE_SHADING_RATE_TIER_2) != 0;

        // Check for Barycentrics support
        D3D12_FEATURE_DATA_D3D12_OPTIONS3 featureDataOptions3 = {};
        ThrowIfFailed(m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS3,
                                                     &featureDataOptions3,
                                                     sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS3)));
        m_barycentricsSupported = featureDataOptions3.BarycentricsSupported;

        InitializeDeviceResources();
    }
    void DX12Device::OnDestroy()
    {

    }

    void DX12Device::InitializeDeviceResources()
    {
        // Create Queue
        {
            m_graphicsQueue = std::make_unique<
                DX12Queue>(m_pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);
            m_computeQueue = std::make_unique<
                DX12Queue>(m_pDevice, D3D12_COMMAND_LIST_TYPE_COMPUTE);
            m_copyQueue = std::make_unique<DX12Queue>(m_pDevice, D3D12_COMMAND_LIST_TYPE_COPY);
        }

        // Create Descriptor Heap
        {
            m_RTVStagingDescriptorHeap = std::make_unique<DX12StagingDescriptorHeap>(
                m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                NUM_RTV_STAGING_DESCRIPTORS);
            m_SRVStagingDescriptorHeap = std::make_unique<DX12StagingDescriptorHeap>(
                m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                NUM_SRV_STAGING_DESCRIPTORS);
            m_DSVStagingDescriptorHeap = std::make_unique<DX12StagingDescriptorHeap>(
                m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
                NUM_DSV_STAGING_DESCRIPTORS);

            for (UINT currFrameIndex = 0; currFrameIndex < NUM_FRAMES_IN_FLIGHT; ++currFrameIndex)
            {
                m_SRVRenderPassDescriptorHeaps[currFrameIndex] = std::make_unique<
                    DX12RenderPassDescriptorHeap>(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                                                  NUM_RESERVED_SRV_DESCRIPTORS,
                                                  NUM_SRV_RENDER_PASS_USER_DESCRIPTORS);

                m_ImguiDescriptors[currFrameIndex] = m_SRVRenderPassDescriptorHeaps[currFrameIndex]
                    ->GetReservedDescriptor(IMGUI_RESERVED_DESCRIPTOR_INDEX);
            }

            m_samplerRenderPassDescriptorHeap = std::make_unique<DX12RenderPassDescriptorHeap>(
                m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
                0, NUM_SAMPLER_DESCRIPTORS);
        }

        // Create Upload Context
        {
            BufferCreationDesc uploadBufferDesc{};
            uploadBufferDesc.accessFlags = BufferAccessFlags::HostWritable;
            uploadBufferDesc.size = 40 * 4096 * 4096;

            BufferCreationDesc uploadTextureDesc{};
            uploadTextureDesc.accessFlags = BufferAccessFlags::HostWritable;
            uploadTextureDesc.size = 40 * 4096 * 4096;

            for (UINT currFrameIndex = 0; currFrameIndex < NUM_FRAMES_IN_FLIGHT; ++currFrameIndex)
            {
                m_uploadContexts[currFrameIndex] = std::make_unique<DX12UploadContext>(this);
            }
        }

        CreateSamplers();

        m_freeReservedDescriptorIndices.resize(NUM_RESERVED_SRV_DESCRIPTORS - 1);
        std::iota(m_freeReservedDescriptorIndices.begin(), m_freeReservedDescriptorIndices.end(),
                  1);
    }

    std::unique_ptr<DX12GraphicsContext> DX12Device::CreateGraphicsContext()
    {
        auto graphicsContext = std::make_unique<DX12GraphicsContext>(this);

        return graphicsContext;
    }
    std::unique_ptr<DX12Shader> DX12Device::CreateShader(ShaderCreateDesc& shaderCreateDesc)
    {
        assert(shaderCreateDesc.stages.size());
        /// Enable Debug
#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags =
            D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_ENABLE_STRICTNESS;
#else
        UINT compileFlags = 0;
#endif

        //
        // Get x64 path
        WCHAR assetsPath[512];
        ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));

        ShaderCompileOptions compileOptions;

#if defined(_DEBUG)
        compileOptions.EnableDebug(true);
        compileOptions.SetOptLevel(0);
#else
        compileOptions.EnableDebug(false);
        compileOptions.SetOptLevel(3);
#endif

        CComPtr<IDxcUtils> pUtils;
        CComPtr<IDxcCompiler3> pCompiler;
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils)));
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler)));

        //
        // Open source file.  
        //
        CComPtr<IDxcBlobEncoding> pSource;
        auto path = ElysiaHelper::GetAssetFullPath(assetsPath,
                                                   shaderCreateDesc.stages.begin()->ShaderName.
                                                                    c_str());
        ThrowIfFailed(pUtils->LoadFile(
            ElysiaHelper::GetAssetFullPath(assetsPath,
                                           shaderCreateDesc.stages.begin()->ShaderName.c_str()).
            c_str(), nullptr, &pSource));
        DxcBuffer Source
        {
            .Ptr = pSource->GetBufferPointer(),
            .Size = pSource->GetBufferSize(),
            .Encoding = DXC_CP_ACP // Assume BOM says UTF8 or UTF16 or this is ANSI text.
        };

        std::wstring hlslWString = StringToWstring((const char*)Source.Ptr);
        auto pragmaInfo = ParseShaderPragmas(hlslWString);
        auto renderStates = ParseShaderRenderPragmas(hlslWString);
        for (auto& stage : shaderCreateDesc.stages)
        {
            switch (stage.ShaderType)
            {
            case ShaderType::Vertex:
            {
                stage.EntryPoint = renderStates.at(L"Vertex");
                break;
            }
            case ShaderType::Pixel:
            {
                stage.EntryPoint = renderStates.at(L"Pixel");
                break;
            }
            case ShaderType::Compute:
            {
                if (stage.EntryPoint.empty())
                {
                    stage.EntryPoint = renderStates.at(L"Compute");
                }
                break;
            }
            }
        }

        auto pKeywordSpace = std::make_unique<ShaderKeywordSpace>();
        for (auto& group : pragmaInfo.KeywordGroups)
        {
            for (auto& key : group.Keywords)
            {
                if (!key.empty())
                {
                    pKeywordSpace->AddKeyword(key);
                }
            }
        }

        auto variantMgr = std::make_unique<ShaderVariantManager>(pKeywordSpace.get());
        variantMgr->SetCompileCallback(
            [this, shaderCreateDesc, compileOptions, Source, ks = pKeywordSpace.get()](
            const ShaderKeywordSet& set)
            {
                return CompileVariantAllStages(compileOptions, shaderCreateDesc, Source, set, ks);
            });

        auto variants = variantMgr->BuildAllVariants(pragmaInfo);
        variantMgr->InitializeFromCompiled(std::move(variants));

        auto o = std::make_unique<DX12Shader>(std::move(variantMgr), std::move(pKeywordSpace));
        o->SetRenderStates(renderStates);
        if (shaderCreateDesc.stages[0].ShaderType != ShaderType::Compute)
            o->BakeVertexLayout();

        return o;
    }
    void DX12Device::CreateSamplers(D3D12_SHADER_VISIBILITY shaderVisibility)
    {
        D3D12_SAMPLER_DESC samplerDescs[NUM_SAMPLER_DESCRIPTORS]{};
        for (size_t i = 0; i < NUM_SAMPLER_DESCRIPTORS; ++i)
        {
            samplerDescs[0].BorderColor[0] =
                samplerDescs[0].BorderColor[1] =
                samplerDescs[0].BorderColor[2] = samplerDescs[0].BorderColor[3] = 0.0f;
            samplerDescs[i].MipLODBias = 0;
            samplerDescs[i].MaxAnisotropy = 16;
            samplerDescs[i].ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
            samplerDescs[i].MinLOD = 0;
            samplerDescs[i].MaxLOD = D3D12_FLOAT32_MAX;
        }
        UINT samplerIndex = 0;
        samplerDescs[samplerIndex].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        //rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
        samplerIndex ++;

        samplerDescs[samplerIndex].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        //rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
        samplerIndex ++;

        samplerDescs[samplerIndex].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        //rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
        samplerIndex ++;

        samplerDescs[samplerIndex].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        //rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
        samplerIndex ++;

        samplerDescs[samplerIndex].Filter = D3D12_FILTER_ANISOTROPIC;
        samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        //rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
        samplerIndex ++;

        samplerDescs[samplerIndex].Filter = D3D12_FILTER_ANISOTROPIC;
        samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDescs[samplerIndex].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        //rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
        samplerIndex ++;

        samplerDescs[samplerIndex].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDescs[samplerIndex].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        //rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
        samplerIndex ++;

        samplerDescs[samplerIndex].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDescs[samplerIndex].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        //rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
        samplerIndex ++;

        auto samplerDescriptorBlock = m_samplerRenderPassDescriptorHeap->
            AllocateRenderPassDescriptorBlock(NUM_SAMPLER_DESCRIPTORS);
        D3D12_CPU_DESCRIPTOR_HANDLE currentSamplerDescriptor = samplerDescriptorBlock.
            GetCPUHandle();

        for (uint32_t samplerIndex = 0; samplerIndex < NUM_SAMPLER_DESCRIPTORS; samplerIndex ++)
        {
            m_pDevice->CreateSampler(&samplerDescs[samplerIndex], currentSamplerDescriptor);
            currentSamplerDescriptor.ptr += m_samplerRenderPassDescriptorHeap->
                GetDescriptorSingleSize();
        }
    }
    void DX12Device::CreateRootParameters(DX12RootSignature* rootSignature,
                                          std::vector<DX12RootParameter*>& rootParamters)
    {
        for (auto i = 0; i < rootParamters.size(); ++i)
        {
            (*rootSignature)[i] = *rootParamters[i];
        }
    }
    DX12RootSignature* DX12Device::CreateRootSignature(const PipelineResourceLayout& resourceLayout,
                                                       PipelineResourceMapping& resourceMapping)
    {
        std::vector<DX12RootParameter*> rootParameters{};
        std::array<std::vector<D3D12_DESCRIPTOR_RANGE1>, NUM_RESOURCE_SPACES> desciptorRanges;

        for (UINT currSpaceID = 0; currSpaceID < NUM_RESOURCE_SPACES; ++currSpaceID)
        {
            auto currSpace = resourceLayout.m_spaces[currSpaceID];
            if (!currSpace)
                continue;

            if (currSpace->IsPushConstantSpace() && (currSpaceID == PER_MATERIAL_SPACE || currSpaceID == PER_OBJECT_SPACE)) 
            {
                DX12RootParameter* rootParameter = new DX12RootParameter();
                rootParameter->InitAsConstants(16, 0, currSpaceID, D3D12_SHADER_VISIBILITY_ALL);

                resourceMapping.m_PushConstantMappings[currSpaceID] = static_cast<UINT>(rootParameters.size());
                rootParameters.emplace_back(std::move(rootParameter));
                continue; // 处理完常量后跳过后续 Table 处理
            }
            
            std::vector<D3D12_DESCRIPTOR_RANGE1>& currDescriptorRange = desciptorRanges[
                currSpaceID];

            auto SRVs = currSpace->GetSRVs();
            auto UAVs = currSpace->GetUAVs();

            if (currSpace->HasExpectedCBV())
            {
                DX12RootParameter* rootParameter = new DX12RootParameter();
                rootParameter->
                    InitAsConstantBufferView(0, D3D12_SHADER_VISIBILITY_ALL, currSpaceID);

                resourceMapping.m_CBVMappings[currSpaceID] = static_cast<UINT>(rootParameters.
                    size());
                rootParameters.emplace_back(std::move(rootParameter));
            }

            if (SRVs.empty() && UAVs.empty())
            {
                continue;
            }

            for (auto& uav : UAVs)
            {
                D3D12_DESCRIPTOR_RANGE1 range{};
                range.BaseShaderRegister = uav->m_bindingIndex;
                range.NumDescriptors = 1;
                range.OffsetInDescriptorsFromTableStart = static_cast<uint32_t>(currDescriptorRange.
                    size());
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                range.RegisterSpace = currSpaceID;
                range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE |
                              D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

                currDescriptorRange.push_back(range);
            }

            // all of SRV Resource has one DESCRIPTOR RANGE which only has one descriptor
            for (auto& SRV : SRVs)
            {
                D3D12_DESCRIPTOR_RANGE1 pDescriptorRange{};
                pDescriptorRange.BaseShaderRegister = SRV->m_bindingIndex;
                pDescriptorRange.NumDescriptors = 1;
                pDescriptorRange.OffsetInDescriptorsFromTableStart = static_cast<UINT>(
                    currDescriptorRange.size());
                pDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                pDescriptorRange.RegisterSpace = currSpaceID;
                pDescriptorRange.Flags =
                    D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE |
                    D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

                currDescriptorRange.emplace_back(pDescriptorRange);
            }

            DX12RootParameter* rootParameter = new DX12RootParameter();
            rootParameter->InitAsDescriptorTable(static_cast<UINT>(currDescriptorRange.size()),
                                                 currDescriptorRange.data(),
                                                 D3D12_SHADER_VISIBILITY_ALL);

            resourceMapping.m_TableMappings[currSpaceID] = static_cast<UINT>(rootParameters.size());
            rootParameters.emplace_back(std::move(rootParameter));
        }

        UINT numRootParamter = static_cast<UINT>(rootParameters.size());
        UINT numSampler = 0;
        DX12RootSignature* rootSignature = new DX12RootSignature(numRootParamter, numSampler);

        CreateRootParameters(rootSignature, rootParameters);

        rootSignature->Init(m_pDevice,
                            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                            D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
                            D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED);

        return rootSignature;
    }

    void DX12Device::CopyDescriptors(uint32_t numDestDescriptorRanges,
                                     const D3D12_CPU_DESCRIPTOR_HANDLE* destDescriptorRangeStarts,
                                     const uint32_t* destDescriptorRangeSizes,
                                     uint32_t numSrcDescriptorRanges,
                                     const D3D12_CPU_DESCRIPTOR_HANDLE* srcDescriptorRangeStarts,
                                     const uint32_t* srcDescriptorRangeSizes,
                                     D3D12_DESCRIPTOR_HEAP_TYPE descriptorType)
    {
        m_pDevice->CopyDescriptors(numDestDescriptorRanges, destDescriptorRangeStarts,
                                   destDescriptorRangeSizes, numSrcDescriptorRanges,
                                   srcDescriptorRangeStarts, srcDescriptorRangeSizes,
                                   descriptorType);
    }
    /// <summary>
    /// 
    /// </summary>
    /// <param name="SRVHandle"> stage SRV Handle in buffer or tex </param>
    /// <param name="index"> resource index in render pass heap </param> 
    void DX12Device::CopyDescriptorFromStageToRenderPass(DX12DescriptorHeapHandle SRVHandle,
                                                         UINT index)
    {
        for (UINT currFrameIndex = 0; currFrameIndex < NUM_FRAMES_IN_FLIGHT; ++currFrameIndex)
        {
            auto targetDescriptor = m_SRVRenderPassDescriptorHeaps[currFrameIndex]->
                GetReservedDescriptor(index);
            m_pDevice->CopyDescriptorsSimple(1, targetDescriptor.GetCPUHandle(),
                                             SRVHandle.GetCPUHandle(),
                                             D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }
    }

    UINT DX12Device::AllocateContiguousReservedDescriptorIndices(UINT count)
    {
        if (count == 0)
            return 0;
        if (m_freeReservedDescriptorIndices.size() < count)
        {
            ElysiaHelper::AssertError("Out of reserved descriptors!");
            return UINT_MAX;
        }

        // 1. ���ĩβ�Ƿ�����
        // m_freeReservedDescriptorIndices ������ Stack��back() �����һ����������
        // ���������Ҫ 3 ������ vector �� [..., 10, 11, 12]���� back �� 12��
        // ������Ҫ��� vector[size-1], vector[size-2]... �Ƿ��������ݼ���
        bool isContiguous = true;
        size_t size = m_freeReservedDescriptorIndices.size();
        UINT lastVal = m_freeReservedDescriptorIndices.back();

        // ���ټ�飺��� (���һ��ֵ - count + 1) ���� (������ count ��ֵ)
        // ˵�����������ֵ�������� (ǰ�����б�ֲ�����iota ��ʼ�����������)
        if (m_freeReservedDescriptorIndices[size - count] != (lastVal - count + 1))
        {
            isContiguous = false;
        }

        // 2. �������������������Ƭ���ͷţ�����������
        if (!isContiguous)
        {
            // ���ܾ��棺Sort ���������������������׶�ͨ���ɽ���
            // ���Ƶ������/���� Mipmap ������������� offset ��������Է�����
            std::sort(m_freeReservedDescriptorIndices.begin(), m_freeReservedDescriptorIndices.end());

            // �ٴμ��
            lastVal = m_freeReservedDescriptorIndices.back();
            if (m_freeReservedDescriptorIndices[size - count] != (lastVal - count + 1))
            {
                ElysiaHelper::AssertError("Heap Fragmentation: Cannot find contiguous block for Mipmaps!");
                return UINT_MAX;
            }
        }

        // 3. ִ�з���
        // ���ǵ� BaseIndexӦ������һ������С���Ǹ���
        UINT baseIndex = lastVal - count + 1;

        // �Ƴ���� count ��Ԫ��
        m_freeReservedDescriptorIndices.resize(size - count);

        return baseIndex;

    }
    void DX12Device::FreeContiguousReservedDescriptorIndices(UINT baseIndex, UINT count)
    {
        for (UINT i = 0; i < count; ++i)
        {
            m_freeReservedDescriptorIndices.push_back(baseIndex + i);
        }
    }

    ContextSubmissionResult DX12Device::SubmitContextWork(DX12Context& context)
    {
        uint64_t fenceResult = 0;

        switch (context.GetContextType())
        {
        case D3D12_COMMAND_LIST_TYPE_DIRECT:
            fenceResult = m_graphicsQueue->ExecuteCommandList(context.GetCommandList());
            break;
        case D3D12_COMMAND_LIST_TYPE_COMPUTE:
            fenceResult = m_computeQueue->ExecuteCommandList(context.GetCommandList());
            break;
        case D3D12_COMMAND_LIST_TYPE_COPY:
            fenceResult = m_copyQueue->ExecuteCommandList(context.GetCommandList());
            break;
        default:
            ElysiaHelper::AssertError("Unsupported submission type.");
        }

        ContextSubmissionResult submissionResult;
        submissionResult.frameID = m_frameID;
        submissionResult.submissionIndex = static_cast<UINT>(m_contextSubmissions[m_frameID].
            size());

        m_contextSubmissions[m_frameID].push_back(
            std::make_pair(fenceResult, context.GetContextType()));

        return submissionResult;
    }

    void DX12Device::DestoryContext(std::unique_ptr<DX12Context> context, UINT frameID)
    {
        m_destructionQueues[frameID].m_contexts.push_back(std::move(context));
    }
    void DX12Device::DestoryBuffer(std::unique_ptr<DX12BufferResource> buffer, UINT frameID)
    {
        m_destructionQueues[frameID].m_buffers.push_back(std::move(buffer));
    }
    void DX12Device::DestoryPipelineState(std::unique_ptr<DX12PipelineState> pipelineState,
                                          UINT frameID)
    {
        m_destructionQueues[frameID].m_pipelineStates.push_back(std::move(pipelineState));
    }
    void DX12Device::DestoryTexture(std::unique_ptr<DX12TextureResource> texture, UINT frameID)
    {
        m_destructionQueues[frameID].m_textures.push_back(std::move(texture));
    }

    void DX12Device::ProcessDestruction(UINT frameID)
    {
        auto& currFrameDestrctuionQueue = m_destructionQueues[frameID];

        (currFrameDestrctuionQueue.m_contexts).clear();
        (currFrameDestrctuionQueue.m_buffers).clear();
        (currFrameDestrctuionQueue.m_textures).clear();
        (currFrameDestrctuionQueue.m_pipelineStates).clear();
    }

    void DX12Device::BeginFrame(UINT frameID)
    {
        m_frameID = frameID;
        // wait on fences from 2 frames ago
        auto& fenceValue = m_endOfFrameFences[m_frameID];
        m_graphicsQueue->WaitForFenceCPUBlocking(fenceValue.m_graphicsQueueFence);
        m_copyQueue->WaitForFenceCPUBlocking(fenceValue.m_copyQueueFence);
        m_computeQueue->WaitForFenceCPUBlocking(fenceValue.m_computeQueueFence);

        ProcessDestruction(m_frameID);

        m_uploadContexts[m_frameID]->ResolveProcessedUploads();
        m_uploadContexts[m_frameID]->Reset();

        BufferManager::GetInstance().GetUploadRingBuffer()->Reset(m_frameID);

        m_contextSubmissions[m_frameID].clear();
    }

    void DX12Device::EndFrame()
    {
        m_uploadContexts[m_frameID]->ProcessUploads();
        SubmitContextWork(*m_uploadContexts[m_frameID]);

        m_endOfFrameFences[m_frameID].m_copyQueueFence = m_copyQueue->SingalFence();
        m_endOfFrameFences[m_frameID].m_computeQueueFence = m_computeQueue->SingalFence();
    }

    void DX12Device::Present()
    {
        m_endOfFrameFences[m_frameID].m_graphicsQueueFence = m_graphicsQueue->SingalFence();
    }

    void DX12Device::WaitForIdle()
    {
        m_graphicsQueue->WaitForIdle();
        m_copyQueue->WaitForIdle();
        m_computeQueue->WaitForIdle();
    }

    ShaderReflectionData DX12Device::ReflectShaderStage(CComPtr<IDxcResult> pResults,
                                                        CComPtr<IDxcUtils> pUtils)
    {
        ShaderReflectionData o{};

        //
        // Get separate reflection.
        //
        CComPtr<IDxcBlob> pReflectionData;
        CComPtr<ID3D12ShaderReflection> pReflection;
        ThrowIfFailed(pResults->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&pReflectionData),
                                          nullptr));
        if (pReflectionData != nullptr)
        {
            // Optionally, save reflection blob for later here.

            // Create reflection interface.
            const DxcBuffer ReflectionData
            {
                .Ptr = pReflectionData->GetBufferPointer(),
                .Size = pReflectionData->GetBufferSize(),
                .Encoding = DXC_CP_ACP,
            };

            pUtils->CreateReflection(&ReflectionData, IID_PPV_ARGS(&pReflection));

            // Use reflection interface here.
            D3D12_SHADER_DESC pShaderDesc{};
            pReflection->GetDesc(&pShaderDesc);

            pReflection->GetThreadGroupSize(&o.ThreadGroupSize.X, &o.ThreadGroupSize.Y, &o.ThreadGroupSize.Z);

            // Set ConstantBuffer layout & constant buffer member
            {
                std::unordered_map<UINT32, ShaderReflectionData::ShaderVariable> shaderVariables{};
                for (UINT i = 0; i < pShaderDesc.BoundResources; ++i)
                {
                    D3D12_SHADER_INPUT_BIND_DESC resourceDesc{};
                    pReflection->GetResourceBindingDesc(i, &resourceDesc);

                    if (resourceDesc.Type == D3D_SIT_CBUFFER)
                    {
                        ID3D12ShaderReflectionConstantBuffer* pConstantBuffer = pReflection->
                            GetConstantBufferByIndex(i);
                        D3D12_SHADER_BUFFER_DESC constantBufferDesc{};
                        pConstantBuffer->GetDesc(&constantBufferDesc);

                        auto variableName = constantBufferDesc.Name;
                        D3D_SHADER_INPUT_TYPE resourceType = resourceDesc.Type;
                        auto spaceID = resourceDesc.Space;
                        auto registerPos = resourceDesc.BindPoint;

                        ShaderReflectionData::ShaderVariable temp
                        {
                            .type = resourceDesc.Type,
                            .bindPoint = registerPos,
                            .spaceID = spaceID,
                            .name = variableName,
                            .size = constantBufferDesc.Size
                        };
                        shaderVariables.insert({temp.spaceID, temp});

                        for (UINT memberIndex = 0; memberIndex < constantBufferDesc.Variables; ++
                             memberIndex)
                        {
                            auto memberVariable = pConstantBuffer->GetVariableByIndex(memberIndex);
                            D3D12_SHADER_VARIABLE_DESC variableDesc{};
                            memberVariable->GetDesc(&variableDesc);

                            ShaderReflectionData::ShaderConstantVariableDesc constantVariableDesc{};
                            constantVariableDesc.SpaceID = spaceID;
                            constantVariableDesc.StartOffset = variableDesc.StartOffset;
                            constantVariableDesc.Size = variableDesc.Size;
                            constantVariableDesc.Name = PropertyToID(
                                StringToWstring(variableDesc.Name));
                            // #ifdef _DEBUG
                            // 							std::cout << "Constant variable name is " << variableDesc.Name << std::endl;
                            // 							std::cout << "Constant variable name hash is " << constantVariableDesc.Name << std::endl;
                            // 							std::cout << "Space ID is " << constantVariableDesc.SpaceID << std::endl;
                            // 							std::cout << "Start Offset is " << constantVariableDesc.StartOffset << std::endl;
                            // 							std::cout << "Size is " << constantVariableDesc.Size << std::endl;
                            // #endif

                            shaderVariables[temp.spaceID].members.emplace(
                                constantVariableDesc.Name, std::move(constantVariableDesc));
                        }
                    }
                }
                o.cbuffers = std::move(shaderVariables);
            }

            // Get Vertex layout
            {
                std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDesc{};
                inputElementDesc.reserve(pShaderDesc.InputParameters);
                std::vector<std::string> inputElementSemanticNames{};
                inputElementSemanticNames.reserve(pShaderDesc.InputParameters);

                for (UINT32 parameterIndex = 0; parameterIndex < pShaderDesc.InputParameters; ++
                     parameterIndex)
                {
                    D3D12_SIGNATURE_PARAMETER_DESC signatureParameterDesc{};
                    ThrowIfFailed(
                        pReflection->GetInputParameterDesc(parameterIndex,
                                                           &signatureParameterDesc));

                    inputElementSemanticNames.emplace_back(
                        signatureParameterDesc.SemanticName
                            ? signatureParameterDesc.SemanticName
                            : std::string());

                    inputElementDesc.emplace_back(D3D12_INPUT_ELEMENT_DESC
                    {
                        .SemanticName = nullptr, //inputElementSemanticNames.back().c_str(),
                        .SemanticIndex = signatureParameterDesc.SemanticIndex,
                        .Format = MaskToFormat(signatureParameterDesc.Mask),
                        .InputSlot = 0u,
                        .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
                        .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                        // There doesn't seem to be a obvious way to 
                        // automate this currently, which might be a issue when instanced rendering is used
                        .InstanceDataStepRate = 0u
                    });

                }

                o.InputElementSemanticNames = std::move(inputElementSemanticNames);
                o.InputLayoutElementDescs = std::move(inputElementDesc);
                o.InputLayoutDesc = D3D12_INPUT_LAYOUT_DESC
                {
                    .pInputElementDescs = o.InputLayoutElementDescs.data(),
                    .NumElements = static_cast<UINT32>(o.InputLayoutElementDescs.size()),
                };
            }
        }

        return o;
    }

    ShaderBytecode DX12Device::CompileShaderStage(
        const std::wstring& path,
        const std::wstring& entry,
        const std::wstring& target,
        const std::vector<LPCWSTR>& args,
        const DxcBuffer& sourceBuffer)
    {
        // Validate args (no null pointers)
        for (size_t i = 0; i < args.size(); ++i)
        {
            if (args[i] == nullptr)
            {
                ThrowRuntimeError(
                    "CompileShaderStage: args contains nullptr at index " + std::to_string(i));
            }
        }

        // 
        // Create compiler and utils.
        //
        CComPtr<IDxcUtils> pUtils;
        CComPtr<IDxcCompiler3> pCompiler;
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils)));
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler)));

        //
        // Create default include handler
        //
        CComPtr<IDxcIncludeHandler> pIncludeHandler;
        auto hr = pUtils->CreateDefaultIncludeHandler(&pIncludeHandler);
        if (FAILED(hr) || !pIncludeHandler)
        {
            // Not fatal, but warn
            std::wstring msg = FormatHrMessage(hr);
            std::wcerr << L"[Warning] CreateDefaultIncludeHandler failed: " << msg << L"\n";
            // we still continue (pIncludeHandler may be nullptr)
        }

        auto pszArgs = args;

        //
        // Compile it with specified arguments.
        //
        CComPtr<IDxcResult> pResults;
        hr = pCompiler->Compile(
            &sourceBuffer,          // Source buffer.
            pszArgs.data(),         // Array of pointers to arguments.
            (UINT)pszArgs.size(),   // Number of arguments.
            pIncludeHandler,        // User-provided interface to handle #include directives (optional).
            IID_PPV_ARGS(&pResults) // Compiler output status, buffer, and errors.
            );
        if (FAILED(hr))
        {
            std::wstring hrmsg = FormatHrMessage(hr);
            std::wostringstream oss;
            oss << L"DXC Compile call failed for " << path << L" entry=" << entry << L" target=" <<
                target << L"\n";
            oss << L"HRESULT: 0x" << std::hex << hr << L" (" << hrmsg << L")\n";
            // also try to get any error text returned in pResults (sometimes present even if Compile returned failure)
            if (pResults)
            {
                CComPtr<IDxcBlobUtf8> pErrors;
                if (SUCCEEDED(
                    pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr)) && pErrors
                    && pErrors->GetStringLength() > 0)
                {
                    oss << L"Compiler errors/warnings:\n" << (const char*)pErrors->
                        GetStringPointer() << L"\n";
                }
            }
            ThrowRuntimeError(
                std::string("DXC compile call failed: ") + WstringToString(oss.str()));
        }

        //
        // Print errors if present.
        //
        CComPtr<IDxcBlobUtf8> pErrors = nullptr;
        pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
        // Note that d3dcompiler would return null if no errors or warnings are present.
        // IDxcCompiler3::Compile will always return an error buffer, but its length
        // will be zero if there are no warnings or errors.
        if (pErrors != nullptr && pErrors->GetStringLength() != 0)
            wprintf(L"Warnings and Errors:\n%S\n", pErrors->GetStringPointer());

        //
        // Quit if the compilation failed.
        //
        HRESULT hrStatus;
        pResults->GetStatus(&hrStatus);
        if (FAILED(hrStatus))
        {
            wprintf(L"Compilation Failed\n");
        }

        //
        // Save shader binary.
        //
        CComPtr<IDxcBlob> pShader = nullptr;
        CComPtr<IDxcBlobUtf16> pShaderName = nullptr;
        pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShader), nullptr);
        if (pShader != nullptr)
        {
            FILE* fp = NULL;

            /*_wfopen_s(&fp, pShaderName->GetStringPointer(), L"wb");
            fwrite(pShader->GetBufferPointer(), pShader->GetBufferSize(), 1, fp);
            fclose(fp);*/

            std::cout << "Shader compiled successfully. Size: " << pShader->GetBufferSize() <<
                " bytes" << std::endl;
            std::cout << "Shader compiled successfully. Adress: " << pShader->GetBufferPointer() <<
                std::endl;
        }

        //
        // Save pdb.
        //
        CComPtr<IDxcBlob> pPDB = nullptr;
        CComPtr<IDxcBlobUtf16> pPDBName = nullptr;
        pResults->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pPDB), &pPDBName);
        if(pPDB != nullptr && pPDBName != nullptr)
        {
            FILE* fp = NULL;

            // Note that if you don't specify -Fd, a pdb name will be automatically generated.
            // Use this file name to save the pdb so that PIX can find it quickly.
            _wfopen_s(&fp, pPDBName->GetStringPointer(), L"wb");
            fwrite(pPDB->GetBufferPointer(), pPDB->GetBufferSize(), 1, fp);
            fclose(fp);
        }

        //
        // Print hash.
        //
        CComPtr<IDxcBlob> pHash = nullptr;
        pResults->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(&pHash), nullptr);
        if (pHash != nullptr && pHash->GetBufferSize() >= 16)
        {
            wprintf(L"Hash: ");
            DxcShaderHash* pHashBuf = (DxcShaderHash*)pHash->GetBufferPointer();
            for (int i = 0; i < _countof(pHashBuf->HashDigest); i ++)
                wprintf(L"%.2x", pHashBuf->HashDigest[i]);
            wprintf(L"\n");
        }

        //
        // Demonstrate getting the hash from the PDB blob using the IDxcUtils::GetPDBContents API
        //
        CComPtr<IDxcBlob> pHashDigestBlob = nullptr;
        CComPtr<IDxcBlob> pDebugDxilContainer = nullptr;
        if (SUCCEEDED(pUtils->GetPDBContents(pPDB, &pHashDigestBlob, &pDebugDxilContainer)))
        {
            // This API returns the raw hash digest, rather than a DxcShaderHash structure.
            // This will be the same as the DxcShaderHash::HashDigest returned from
            // IDxcResult::GetOutput(DXC_OUT_SHADER_HASH, ...).
            wprintf(L"Hash from PDB: ");
            const BYTE* pHashDigest = (const BYTE*)pHashDigestBlob->GetBufferPointer();
            assert(pHashDigestBlob->GetBufferSize() == 16); // hash digest is always 16 bytes.
            for (int i = 0; i < pHashDigestBlob->GetBufferSize(); i ++)
                wprintf(L"%.2x", pHashDigest[i]);
            wprintf(L"\n");

            // The pDebugDxilContainer blob will contain a DxilContainer formatted
            // binary, but with different parts than the pShader blob retrieved
            // earlier.
            // The parts in this container will vary depending on debug options and
            // the compiler version.
            // This blob is not meant to be directly interpreted by an application.
        }

        ShaderBytecode o
        {
            .bytecode = pShader,
            .entry = entry,
            .target = target,
            .ReflectionData = ReflectShaderStage(pResults, pUtils),
            .args = args
        };

        return o;
    }

    ShaderVariantData DX12Device::CompileVariantAllStages(
        const ShaderCompileOptions& compileOptions,
        const ShaderCreateDesc& desc,
        const DxcBuffer& source,
        const ShaderKeywordSet& keywordSet,
        const ShaderKeywordSpace* keywordSpace)
    {
        ShaderVariantData o{};
        o.KeywordSet = keywordSet;

        for (auto& stage : desc.stages)
        {
            /// Switch Target
            std::wstring target;
            switch (stage.ShaderType)
            {
            case ShaderType::Vertex:
            {
                target = L"vs_6_6";
            }

            break;
            case ShaderType::Pixel:
            {
                target = L"ps_6_6";
            }
            break;
            case ShaderType::Compute:
            {
                target = L"cs_6_6";
            }
            break;

            default:
                ElysiaHelper::AssertError("Unimplemented shader type.");
                break;
            }

            //
            // Get x64 path
            WCHAR assetsPath[512];
            ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));

            std::cout << std::filesystem::path(stage.ShaderName).string() << std::endl;
            std::cout << std::filesystem::path(stage.EntryPoint).string() << std::endl;
            std::cout << std::filesystem::path(target).string() << std::endl;
            auto temp = std::filesystem::path(assetsPath).wstring();
            temp += L"\\Shaders";

            LPCWSTR pdbName = std::wstring(
                stage.ShaderName + stage.EntryPoint + std::wstring(L".pdb")).c_str();
            LPCWSTR binName = std::wstring(
                stage.ShaderName + stage.EntryPoint + std::wstring(L".bin")).c_str();

            auto newCompileOptions = compileOptions;
            newCompileOptions.SetShaderPath(stage.ShaderName);
            newCompileOptions.SetEntry(stage.EntryPoint);
            newCompileOptions.SetTarget(target);
            newCompileOptions.AddIncludeDir(temp);
            newCompileOptions.AddIncludeDir(temp + L"\\public");
            newCompileOptions.AddIncludeDir(temp + L"\\private");

            for (size_t i = 0; i < keywordSpace->Count(); i ++)
            {
                if (keywordSet.Bits().test(i))
                {
                    auto name = keywordSpace->GetName((int)i);
                    newCompileOptions.AddMacro(name);
                }
            }
            auto pszArgs = newCompileOptions.BuildArguments();

            o.StageShaders[stage.ShaderType] = CompileShaderStage(
                stage.ShaderName, stage.EntryPoint, target, pszArgs, source);
            o.MergedReflectionData.Merge(o.StageShaders[stage.ShaderType].ReflectionData);
        }

        o.pMeshResourceLayout = std::make_unique<PipelineResourceLayout>();
        for (auto& shaderVariable : o.MergedReflectionData.cbuffers)
        {
            auto currVariable = shaderVariable.second;
            UINT spaceID = currVariable.spaceID;

            PipelineResourceSpace* pSpace = o.pMeshResourceLayout->m_spaces[spaceID];
            if (!pSpace)
            {
                pSpace = new PipelineResourceSpace();
                o.pMeshResourceLayout->SetSpace(spaceID, pSpace);
            }
            if (spaceID == PER_MATERIAL_SPACE && currVariable.type == D3D_SIT_CBUFFER) 
            {
                pSpace->ExpectPushConstant(currVariable.bindPoint);
                // 计算需要的 DWORD 数量 (Size 是字节，需除以 4)
                pSpace->SetPushConstantNumDWORDs(CeilDivide(currVariable.size, 4));
            }
            else 
            {
                // 原有逻辑：处理普通资源
                switch (currVariable.type)
                {
                case D3D_SIT_CBUFFER:   pSpace->ExpectCBV(currVariable.bindPoint); break;
                case D3D_SIT_TEXTURE:   pSpace->ExpectSRV(currVariable.bindPoint); break;
                case D3D_SIT_STRUCTURED: pSpace->ExpectUAV(currVariable.bindPoint); break;
                }
            }
        }

        return o;
    }

    ID3D12CommandQueue* DX12Device::GetDirectQueue() const noexcept
    {
        return m_graphicsQueue->GetCommandQueue();
    }
    void DX12Device::GetDeviceInfo(std::string* deviceName, std::string* driverVersion)
    {
        DXGI_ADAPTER_DESC adapterDescription;
        m_pAdapter->GetDesc(&adapterDescription);

        *deviceName = format("%S", adapterDescription.Description);

        if (m_agsContext)
            *driverVersion = m_agsGPUInfo.driverVersion;
        else
            *driverVersion = "Enable AGS for Driver Version";
    }
}