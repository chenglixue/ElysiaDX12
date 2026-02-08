#include "stdafx.h"
#include "GIPass.h"

#include "Editor/UserData.h"
#include "Programs/PIXHelper.h"
#include "Programs/SobolSequenceGenerator.h"
#include "Runtime/Core/DX12GraphicsContext.h"
#include "Runtime/Core/DX12Shader.h"
#include "Runtime/Core/DX12Device.h"
#include "Runtime/Core/DX12TextureBuffer.h"
#include "Runtime/Core/SwapChain.h"
#include "Runtime/Engine/ECS/Entity.h"
#include "Runtime/RenderCore/DX12Camera.h"

#include "Runtime/RenderCore/RenderResource.h"
#include "Runtime/RenderCore/RenderTexture.h"
#include "Runtime/RenderCore/Material.h"
#include "Runtime/RenderCore/PSOManager.h"
#include "Runtime/RenderCore/RenderTargetManager.h"
#include "Runtime/RenderCore/SceneManager.h"
#include "Runtime/RenderCore/ShaderVariantManager.h"


namespace ElysiaRenderer
{
    GIPass::GIPass()
        : BasePass()
    {
        BufferCreationDesc vertexBufferDesc =
        {
            .name = L"GI Vertex Buffer",
            .stride = sizeof(Vector3),
            .size = sizeof(Vector3) * NumVertices,
            .viewFlags = GPUResourceFlags::None,
            .accessFlags = BufferAccessFlags::GPUOnly,
            .isRawAccess = false,
            .InitData = m_probeVertices
        };
        BufferCreationDesc indexBufferDesc =
        {
            .name = L"GI Index Buffer",
            .stride = 0,
            .size = sizeof(INDEX_FORMAT) * NumIndices,
            .viewFlags = GPUResourceFlags::None,
            .accessFlags = BufferAccessFlags::GPUOnly,
            .isRawAccess = false,
            .InitData = m_probeIndices
        };
        m_vertexBuffer = BufferManager::GetInstance().CreateVertexBuffer(vertexBufferDesc);
        m_indexBuffer = BufferManager::GetInstance().CreateIndexBuffer(indexBufferDesc);

        m_vertexView = D3D12_VERTEX_BUFFER_VIEW
        {
            .BufferLocation = m_vertexBuffer->GetGPUAddress(),
            .SizeInBytes = static_cast<UINT>(NumVertices) * m_vertexBuffer->GetStride(),
            .StrideInBytes = m_vertexBuffer->GetStride()
        };
        m_indexView =
        {
            .BufferLocation = m_indexBuffer->GetGPUAddress(),
            .SizeInBytes = NumIndices * ElysiaModel::IndexSize(),
            .Format = ElysiaModel::IndexBufferFormat(),
        };

        m_pRayDataBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"DDGI Ray Data Buffer",
            .stride = sizeof(RayData),
            .size = sizeof(RayData) * Probe_Count * Rays_Per_Probe,
            .viewFlags = GPUResourceFlags::SRV | GPUResourceFlags::UAV,
            .accessFlags = BufferAccessFlags::GPUOnly,
            .isRawAccess = false
        });

        m_DXRBlob = CompileRaytracingLibrary(L"Shaders\\public\\GI\\DDGI_Library.hlsl");
    }

    GIPass::~GIPass()
    {
        Dispose();
    }
    void GIPass::Dispose()
    {
    }

    void GIPass::Configure()
    {
        m_halfWidth = UINT(m_renderSize.x) >> 1;
        m_halfHeight = UINT(m_renderSize.y) >> 1;
        m_quarterWidth = UINT(m_renderSize.x) >> 2;
        m_quarterHeight = UINT(m_renderSize.y) >> 2;

        m_pRayDataBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"Ray Data Buffer",
            .stride = sizeof(RayData),
            .size = sizeof(RayData) * Probe_Count * Rays_Per_Probe,
            .viewFlags = GPUResourceFlags::SRV | GPUResourceFlags::UAV,
            .accessFlags = BufferAccessFlags::GPUOnly,
            .isRawAccess = false
        });

        m_pIrradianceRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            Grid_Dimensions.x * 8,
            Grid_Dimensions.y * Grid_Dimensions.z * 8,
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            true,
            RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::IrradianceRTID));
        m_pDistanceRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            Grid_Dimensions.x * 16,
            Grid_Dimensions.y * Grid_Dimensions.z * 16,
            DXGI_FORMAT_R16G16_FLOAT,
            true,
            RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::DistanceRTID));

        m_shaderPasses.assign(std::begin(m_PassData), std::end(m_PassData));
        m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);

        UpdatePipeline();
    }

    void GIPass::Render(FrameContext& context)
    {
        if (!SceneManager::GetInstance().GetEntities().empty())
        {
            auto& entities = SceneManager::GetInstance().GetEntities()[0];
            auto instanceID = UserData::GetInstance().instanceID;
            instanceID = MathHelper::Max(0, instanceID);
            const Entity* pEntity = nullptr;
            if (instanceID < entities->GetChildren().size())
            {
                pEntity = entities->GetChildren()[instanceID].get();
            }
            else
            {
                pEntity = entities.get();
            }

            auto sceneAABB = pEntity->GetWorldAABB();

            auto sceneMin = sceneAABB.Center - sceneAABB.Extents;
            auto sceneMax = sceneAABB.Center + sceneAABB.Extents;

            // 希望探针能稍微往里面缩一点，不要紧贴墙壁
            float padding = 0.5f;
            Vector3 effectiveMin = sceneMin + Vector3(padding, padding, padding);
            Vector3 effectiveMax = sceneMax - Vector3(padding, padding, padding);
            Vector3 effectiveSize = effectiveMax - effectiveMin;

            // 计算间距 (注意是 数量-1)
            float spacingX = effectiveSize.x / (16 - 1);
            float spacingY = effectiveSize.y / (4 - 1);
            float spacingZ = effectiveSize.z / (16 - 1);

            m_gridSpacing = Vector3(spacingX, spacingY, spacingZ);
            m_gridOrigin = effectiveMin;

        }
        if (!m_vertexBuffer->GetIsReady() || !m_indexBuffer->GetIsReady())
            return;

        PIXHelper pix(m_pCommand->GetCommandList(), "GI Pass");
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;
        m_frameIndex = context.frameIndex;
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), "GI Begin");

        GenerateRay();
    }

    void GIPass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        for (UINT i = 0; i < GI_PASS_COUNT; ++i)
        {
            std::vector<std::wstring> enableKeywords{};

            auto passID = PassID(i);
            auto& passData = m_pMaterial->GetPassData(passID);
            auto VariantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

            passData.pPipelineStateObject =
                PSOManager::GetInstance().GetComputePipelineState(
                    m_pDevice,
                    m_pMaterial.get(),
                    passID);
        }

        if (!m_pRTPSO || !m_pGlobalRootSig)
        {
            assert(m_DXRBlob && m_pDevice->GetDevice());
            {
                CreateDXRRootSignature(m_pDevice->GetDevice());
                CreateRaytracingPipeline(m_pDevice->GetDevice(), m_pGlobalRootSig);
                m_stbHelper.Build(m_pDevice->GetDevice(), m_pRTPSO);
            }
        }

    }

    void GIPass::GenerateRay()
    {
        PIXHelper pix(m_pCommand->GetCommandList(), "Generate Ray");

        m_pCommand->GetCommandList()->SetPipelineState1(m_pRTPSO);
        struct alignas(16)
        {
            Vector4 g_GridSpacing;
            Vector4 g_GridOrigin;
            Vector4 g_GridDimensions;

            uint g_RayDataBufferIndex;
            float g_RandomRotation;
        } constantData;
        constexpr UINT constantSize = sizeof(constantData) / 4;

        m_pCommand->AddBarrier(*m_pRayDataBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        {
            assert(m_pGlobalRootSig != nullptr);
            m_pCommand->GetCommandList()->SetComputeRootSignature(m_pGlobalRootSig);

            constantData =
            {
                .g_GridSpacing = Vector4(m_gridSpacing.x, m_gridSpacing.y, m_gridSpacing.z, 0.f),
                .g_GridOrigin = Vector4(m_gridOrigin.x, m_gridOrigin.y, m_gridOrigin.z, 0.f),
                .g_GridDimensions = Vector4(Grid_Dimensions.x,
                                            Grid_Dimensions.y,
                                            Grid_Dimensions.z,
                                            0.f),
                .g_RayDataBufferIndex = m_pRayDataBuffer->GetResourceHeapIndex(),
                .g_RandomRotation = fmodf(static_cast<float>(m_frameIndex) * k_GoldenAngle,
                                          2.0f * 3.14159265f),
            };

            m_pCommand->GetCommandList()->SetComputeRoot32BitConstants(
                0,
                constantSize,
                &constantData,
                0);

            D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
            dispatchDesc.Width = Probe_Count;
            dispatchDesc.Height = Rays_Per_Probe;
            dispatchDesc.Depth = 1;
            dispatchDesc.RayGenerationShaderRecord = m_stbHelper.GetRayGenRange();
            m_pCommand->GetCommandList()->DispatchRays(&dispatchDesc);
        }
        m_pCommand->AddBarrier(*m_pRayDataBuffer,
                               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), "Generate Ray");
    }

    CComPtr<IDxcBlob> GIPass::CompileRaytracingLibrary(const std::wstring& fileName)
    {
        m_tempStrings.clear();
        m_tempStrings.reserve(20);
        CComPtr<IDxcUtils> pUtils;
        CComPtr<IDxcCompiler3> pCompiler;
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils)));
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler)));

        CComPtr<IDxcIncludeHandler> pIncludeHandler;
        ThrowIfFailed(pUtils->CreateDefaultIncludeHandler(&pIncludeHandler));

        WCHAR assetsPath[512];
        ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));
        auto path = ElysiaHelper::GetAssetFullPath(assetsPath, fileName.c_str());

        // 1. 读取文件内容
        CComPtr<IDxcBlobEncoding> pSource;
        ThrowIfFailed(pUtils->LoadFile(path.c_str(), nullptr, &pSource));

        auto shaderDir = std::filesystem::path(assetsPath).wstring();
        shaderDir += L"\\Shaders";

        // 2. 设置编译参数
        // DXR 必须使用 lib_6_x profile
        std::vector<LPCWSTR> arguments;
        arguments.push_back(L"-Od");
        arguments.push_back(L"-Zi");
        arguments.push_back(L"-Qembed_debug");
        arguments.push_back(L"-Qstrip_reflect");
        arguments.push_back(L"-T");
        arguments.push_back(L"lib_6_6");
        auto addInclude = [&](const std::wstring& relPath)
        {
            m_tempStrings.emplace_back(L"-I" + shaderDir + relPath);
            arguments.push_back(m_tempStrings.back().c_str());
        };
        addInclude(L"");
        addInclude(L"\\public");
        addInclude(L"\\private");
        arguments.push_back(L"-Fd");
        arguments.push_back(L".\\");
        arguments.push_back(L"-Zpr"); // 强制序数排列 (可选)
        arguments.push_back(L"-Zss"); // 建立源码摘要
        // arguments.push_back(L"-fspv-extension=SPV_KHR_non_semantic_info");

        DxcBuffer sourceBuffer;
        sourceBuffer.Ptr = pSource->GetBufferPointer();
        sourceBuffer.Size = pSource->GetBufferSize();
        sourceBuffer.Encoding = DXC_CP_UTF8;

        // 3. 执行编译
        CComPtr<IDxcResult> pResults;
        auto hr = (pCompiler->Compile(&sourceBuffer,
                                      arguments.data(),
                                      (uint32_t)arguments.size(),
                                      pIncludeHandler,
                                      IID_PPV_ARGS(&pResults)));
        if (FAILED(hr))
        {
            std::wstring hrmsg = FormatHrMessage(hr);
            std::wostringstream oss;
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

        // 4. 检查错误
        CComPtr<IDxcBlobUtf8> pErrors;
        pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
        if (pErrors != nullptr && pErrors->GetStringLength() != 0)
            wprintf(L"Warnings and Errors:\n%S\n", pErrors->GetStringPointer());

        HRESULT hrStatus;
        pResults->GetStatus(&hrStatus);
        if (FAILED(hrStatus))
        {
            wprintf(L"Compilation Failed\n");
        }

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
        if (pPDB != nullptr && pPDBName != nullptr)
        {
            std::wstring pdbPath = pPDBName->GetStringPointer();
            std::wstring fullPdbPath = assetsPath + pdbPath;

            // 如果路径包含文件夹，确保文件夹存在
            FILE* fp = NULL;
            _wfopen_s(&fp, fullPdbPath.c_str(), L"wb");
            if (fp)
            {
                fwrite(pPDB->GetBufferPointer(), pPDB->GetBufferSize(), 1, fp);
                fclose(fp);

                // 打印一下路径，确认它是不是在 .exe 旁边
                std::wcout << L"PDB saved to: " << fullPdbPath << std::endl;
            }
        }

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

        assert(pShader);
        return pShader;
    }
    void GIPass::CreateRaytracingPipeline(ID3D12Device* pDevice,
                                          ID3D12RootSignature* pRootSignature)
    {
        CD3DX12_STATE_OBJECT_DESC pipelineDesc(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

        D3D12_SHADER_BYTECODE rayGenBytecode =
        {
            .pShaderBytecode = m_DXRBlob->GetBufferPointer(),
            .BytecodeLength = m_DXRBlob->GetBufferSize()
        };

        auto lib = pipelineDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
        D3D12_SHADER_BYTECODE libBytecode = rayGenBytecode;
        lib->SetDXILLibrary(&libBytecode);
        // 导出你在 HLSL 中定义的入口点名称
        lib->DefineExport(L"GenerateRayMain");

        auto shaderConfig = pipelineDesc.CreateSubobject<
            CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
        // float4 color + float distance = 20 bytes, 建议稍微预留一点空间
        uint32_t maxPayloadSize = 32;
        uint32_t maxAttributeSize = 8; // float2 barycentrics
        shaderConfig->Config(maxPayloadSize, maxAttributeSize);

        auto globalRootSig = pipelineDesc.CreateSubobject<
            CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
        globalRootSig->SetRootSignature(pRootSignature);

        auto pipelineConfig = pipelineDesc.CreateSubobject<
            CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
        // DDGI 通常是一次射出，不需要递归
        pipelineConfig->Config(1);

        D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
        HRESULT hr = pDevice->CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS5,
            &options5,
            sizeof(options5));
        if (FAILED(hr) || options5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
        {
            ThrowRuntimeError("unsupported ray tracing");
        }
        CComPtr<ID3D12Device5> pDevice5;
        ThrowIfFailed(pDevice->QueryInterface(IID_PPV_ARGS(&pDevice5)));

        hr = pDevice5->CreateStateObject(pipelineDesc, IID_PPV_ARGS(&m_pRTPSO));

        if (SUCCEEDED(hr))
        {
            // 构建成功后，提取 Shader ID 用于你的 SBTHelper
            CComPtr<ID3D12StateObjectProperties> pRTProps;
            m_pRTPSO->QueryInterface(IID_PPV_ARGS(&pRTProps));

            void* rayGenID = pRTProps->GetShaderIdentifier(L"GenerateRayMain");

            // 将这些 ID 存入你的 SBTHelper 即可开始 DispatchRays
            m_stbHelper.AddRayGen(rayGenID);
        }
    }
    void GIPass::CreateDXRRootSignature(ID3D12Device* pDevice)
    {
        // 1. 定义根参数：对于 Bindless 方案，我们通常只需要“根常量 (Root Constants)”
        // 用来传递诸如 g_RayDataUAVIndex 或 DDGI 配置结构体的索引
        CD3DX12_ROOT_PARAMETER1 rootParameters[1];

        // 假设我们需要 16 个 32位常量 (比如一个 ViewProj 矩阵或一组索引)
        rootParameters[0].InitAsConstants(16, 0, 2);

        // 2. 核心标志位：必须包含直接索引堆的标志
        D3D12_ROOT_SIGNATURE_FLAGS rsFlags =
            D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
            D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc;
        rsDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rsFlags);

        // 3. 序列化并创建
        CComPtr<ID3DBlob> signature;
        CComPtr<ID3DBlob> error;
        HRESULT hr = D3D12SerializeVersionedRootSignature(&rsDesc, &signature, &error);

        if (FAILED(hr))
        {
            if (error)
            {
                OutputDebugStringA((char*)error->GetBufferPointer());
            }
            return;
        }

        ElysiaHelper::ThrowIfFailed(pDevice->CreateRootSignature(
            0,
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(&m_pGlobalRootSig)));
    }

}