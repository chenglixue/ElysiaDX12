#include "stdafx.h"
#include "../public/SelectionOutlinePass.h"

#include "Editor/public/UserData.h"
#include "Programs/public/PIXHelper.h"
#include "Programs/public/Log.h"

#include "Runtime/Core/public/DX12Device.h"
#include "Runtime/Core/public/DX12GraphicsContext.h"
#include "Runtime/Core/public/DX12Shader.h"
#include "Runtime/Core/public/DX12TextureBuffer.h"

#include "Runtime/Engine/ECS/public/Entity.h"
#include "Runtime/Engine/public/FrameContext.h"

#include "Runtime/RenderCore/public/BufferManager.h"
#include "Runtime/RenderCore/public/Material.h"
#include "Runtime/RenderCore/public/PSOManager.h"
#include "Runtime/RenderCore/public/RenderTargetManager.h"
#include "Runtime/RenderCore/public/RenderTexture.h"
#include "Runtime/RenderCore/public/SelectionManager.h"
#include "Runtime/RenderCore/public/ShaderVariantManager.h"
#include "Runtime/RenderCore/Pass/public/GBufferPass.h"

namespace ElysiaRenderer
{
    namespace
    {
        constexpr DXGI_FORMAT MaskRTFormat = DXGI_FORMAT_R8G8_UNORM;
        constexpr DXGI_FORMAT PickRTFormat = DXGI_FORMAT_R32_FLOAT;

        // CopyTextureRegion requires a 256-byte aligned row pitch; a 1x1 pixel
        // copy therefore needs a 256-byte destination buffer.
        constexpr UINT PickReadbackSize = 256;

        // Rebuild the same TAA-jittered projection GBufferPass used to write the
        // camera depth (SkyboxPass recipe), so the manual depth compare in the
        // Mask/Pick shaders matches the stored depth exactly - otherwise slanted
        // surfaces fail the compare due to the per-pixel depth slope.
        Matrix GetJitteredViewProj(const DX12Camera* pCamera, const Vector2& displaySize)
        {
            Matrix jitteredProj = pCamera->GetProjMat();
            const auto& jitterUV = GBufferPass::m_currJitterUV;
            const float intensity = UserData::GetInstance().taaParameter.jitterIntensity;
            jitteredProj.m[2][0] += jitterUV.x * 2.f / displaySize.x * intensity;
            jitteredProj.m[2][1] -= jitterUV.y * 2.f / displaySize.y * intensity;
            return pCamera->GetViewMat() * jitteredProj;
        }
    }

    SelectionOutlinePass::SelectionOutlinePass()
        : BasePass()
    {
    }

    SelectionOutlinePass::~SelectionOutlinePass()
    {
        Dispose();
    }

    void SelectionOutlinePass::Dispose()
    {
        // RTs are owned by RenderTargetManager and the readback buffer by
        // BufferManager; only clear the references here (GBufferPass precedent)
        m_pMaskRT = nullptr;
        m_pPickRT = nullptr;
        m_pPickReadback = {};
        m_bPickResolvePending = false;
        m_PickEntityLUT.clear();
    }

    void SelectionOutlinePass::Configure()
    {
        m_shaderPasses =
        {
            ShaderPass
            {
                .Name = "Selection Mask Pass",
                .FilePath = L"Shaders\\public\\Editor\\SelectionMask.hlsl",
            },
            ShaderPass
            {
                .Name = "Selection Outline Pass",
                .FilePath = L"Shaders\\public\\Editor\\SelectionOutline.hlsl",
            },
            ShaderPass
            {
                .Name = "Selection Pick Pass",
                .FilePath = L"Shaders\\public\\Editor\\SelectionPick.hlsl",
            },
        };

        if (!m_pMaterial)
        {
            m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        }

        ShaderPassIDs::MaskPassID = m_pMaterial->FindPassIndex("Selection Mask Pass");
        ShaderPassIDs::OutlinePassID = m_pMaterial->FindPassIndex("Selection Outline Pass");
        ShaderPassIDs::PickPassID = m_pMaterial->FindPassIndex("Selection Pick Pass");

        // RTs match CameraColorRT/CameraDepthRT resolution (TAA upscale scale).
        // RenderTargetManager caches by name and recreates on window resize.
        const UINT width = static_cast<UINT>(
            std::floor(m_displaySize.x * UserData::GetInstance().taaParameter.sampleRate));
        const UINT height = static_cast<UINT>(
            std::floor(m_displaySize.y * UserData::GetInstance().taaParameter.sampleRate));

        m_pMaskRT = RenderTargetManager::GetInstance().CreateRenderTexture(
            width,
            height,
            MaskRTFormat,
            false,
            L"Selection Mask RT");

        m_pPickRT = RenderTargetManager::GetInstance().CreateRenderTexture(
            width,
            height,
            PickRTFormat,
            false,
            L"Selection Pick RT");

        if (!m_pPickReadback)
        {
            m_pPickReadback = BufferManager::GetInstance().CreateReadBackBuffer(
                PickReadbackSize,
                "Selection Pick Readback");
        }

        UpdatePipeline();
    }

    void SelectionOutlinePass::Render(ElysiaEngine::FrameContext& context)
    {
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;

        PIXHelper pix(m_pCommand->GetCommandList(), "Selection Outline Pass");

        // Resolve the click from the previous frame first, so a new copy can
        // safely reuse the readback buffer below.
        ResolvePendingPick(context.renderList);

        ElysiaEngine::Entity* pSelected = SelectionManager::GetInstance().GetSelected();
        if (pSelected)
        {
            DrawMask(context, pSelected);
            DrawOutline(context, pSelected);
        }

        // UE HitProxy: render entity IDs only on frames with a pending click.
        Vector2 pickUV = Vector2::Zero;
        if (SelectionManager::GetInstance().ConsumePickRequest(pickUV))
        {
            RenderPick(context, pickUV);
        }
    }

    void SelectionOutlinePass::DrawMask(ElysiaEngine::FrameContext& context,
                                        ElysiaEngine::Entity* pSelected)
    {
        auto passID = ShaderPassIDs::MaskPassID;
        auto& passData = m_pMaterial->GetPassData(passID);
        PIXHelper pix(m_pCommand->GetCommandList(), "Selection Mask Pass");

        // Gather render items of the selected entity
        std::vector<RenderItem*> drawItems;
        drawItems.reserve(context.renderList.size());
        for (auto& renderItem : context.renderList)
        {
            if (renderItem.pAssociatedEntity == pSelected)
            {
                drawItems.push_back(&renderItem);
            }
        }

        if (drawItems.empty())
            return;

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        pipelineStateData.m_renderTargets = {m_pMaskRT->GetTexture()};
        m_pCommand->SetPipeline(pipelineStateData);

        m_pCommand->AddBarrier(m_pMaskRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_pCommand->ClearRenderTarget(m_pMaskRT, Color::Black);

        // The mask PS samples the camera depth, which the engine leaves in
        // NON_PIXEL_SHADER_RESOURCE | DEPTH_READ (for the compute AO/SSSR/TAA
        // passes). A pixel-shader read needs PIXEL_SHADER_RESOURCE, otherwise
        // the depth reads back as 0 and the visibility test always fails.
        m_pCommand->AddBarrier(m_pCameraDepthRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        m_pCommand->SetDefaultViewportAndScissor(
            ElysiaHelper::UINT2(m_pMaskRT->GetWidth(), m_pMaskRT->GetHeight()));
        m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_pCommand->SetVertexBuffer(0, 1,
                                    BufferManager::GetInstance().GetGlobalVertexBufferView());
        m_pCommand->SetIndexBuffer(
            BufferManager::GetInstance().GetGlobalIndexBufferView());

        m_pMaterial->SetMatrix(ShaderIDs::g_ViewProjMatrix,
                               GetJitteredViewProj(m_pCamera, m_displaySize),
                               passID);
        m_pMaterial->SetFloat4(ShaderIDs::g_ScreenSize,
                               GetScreenSize(m_pMaskRT->GetWidth(), m_pMaskRT->GetHeight()),
                               passID);
        m_pMaterial->SetUINT(ShaderIDs::g_DepthTexIndex,
                             m_pCameraDepthRT->GetSRVResourceHeapIndex(),
                             passID);

        // One draw per item, each with its own world matrix re-uploaded to the
        // per-pass constant buffer. Engine convention (GBuffer/Shadow): bind the
        // GLOBAL views and let startIndex/baseVertex carry the per-mesh offsets.
        for (RenderItem* pItem : drawItems)
        {
            m_pMaterial->SetMatrix(ShaderIDs::g_WorldMatrix, pItem->worldMatrix, passID);
            SetSpaceResource(passData, PER_PASS_SPACE);

            m_pCommand->DrawInstanced(pItem->indexCount, 1, pItem->startIndex,
                                      pItem->baseVertex, 0);
        }

        m_pCommand->AddBarrier(m_pMaskRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_pCommand->AddBarrier(m_pCameraDepthRT,
                               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
                               D3D12_RESOURCE_STATE_DEPTH_READ);
    }

    void SelectionOutlinePass::DrawOutline(ElysiaEngine::FrameContext& context,
                                           ElysiaEngine::Entity* pSelected)
    {
        auto passID = ShaderPassIDs::OutlinePassID;
        auto& passData = m_pMaterial->GetPassData(passID);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        pipelineStateData.m_renderTargets = {m_pDisplayRT->GetTexture()};
        m_pCommand->SetPipeline(pipelineStateData);

        m_pMaterial->SetUINT(ShaderIDs::g_MaskTexIndex,
                             m_pMaskRT->GetSRVResourceHeapIndex(),
                             passID);
        m_pMaterial->SetFloat4(ShaderIDs::g_MaskSize,
                               GetScreenSize(m_pMaskRT->GetWidth(), m_pMaskRT->GetHeight()),
                               passID);
        SetSpaceResource(passData, PER_PASS_SPACE);

        m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_displaySize));
        m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_pCommand->AddBarrier(m_pDisplayRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_pCommand->DrawFullScreenTriangle();
        m_pCommand->AddBarrier(m_pDisplayRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), "SelectionOutline");
    }

    void SelectionOutlinePass::RenderPick(ElysiaEngine::FrameContext& context,
                                          const Vector2& viewportUV)
    {
        auto passID = ShaderPassIDs::PickPassID;
        auto& passData = m_pMaterial->GetPassData(passID);
        PIXHelper pix(m_pCommand->GetCommandList(), "Selection Pick Pass");

        // LUT: render item index -> entity, resolved from the readback later
        m_PickEntityLUT.clear();
        m_PickEntityLUT.reserve(context.renderList.size());
        for (auto& renderItem : context.renderList)
        {
            m_PickEntityLUT.push_back(renderItem.pAssociatedEntity);
        }

        if (m_PickEntityLUT.empty())
        {
            SelectionManager::GetInstance().ResolvePick(nullptr);
            return;
        }

        // Drawable items (the id written per item is its render list index,
        // which is also the key into m_PickEntityLUT)
        std::vector<RenderItem*> pickDrawItems;
        std::vector<UINT> pickDrawIds;
        pickDrawItems.reserve(context.renderList.size());
        pickDrawIds.reserve(context.renderList.size());
        for (size_t itemIndex = 0; itemIndex < context.renderList.size(); ++itemIndex)
        {
            RenderItem& renderItem = context.renderList[itemIndex];
            if (!renderItem.pAssociatedEntity)
            {
                continue;
            }
            pickDrawItems.push_back(&renderItem);
            pickDrawIds.push_back(static_cast<UINT>(itemIndex));
        }

        if (pickDrawItems.empty())
        {
            SelectionManager::GetInstance().ResolvePick(nullptr);
            return;
        }

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        pipelineStateData.m_renderTargets = {m_pPickRT->GetTexture()};
        m_pCommand->SetPipeline(pipelineStateData);

        m_pCommand->AddBarrier(m_pPickRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_pCommand->ClearRenderTarget(m_pPickRT, Color::Black);

        // See DrawMask: the pick PS samples the camera depth in a pixel shader.
        m_pCommand->AddBarrier(m_pCameraDepthRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        const UINT pickWidth = static_cast<UINT>(m_pPickRT->GetWidth());
        const UINT pickHeight = static_cast<UINT>(m_pPickRT->GetHeight());
        m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(pickWidth, pickHeight));
        m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_pCommand->SetVertexBuffer(0, 1,
                                    BufferManager::GetInstance().GetGlobalVertexBufferView());
        m_pCommand->SetIndexBuffer(
            BufferManager::GetInstance().GetGlobalIndexBufferView());

        m_pMaterial->SetMatrix(ShaderIDs::g_ViewProjMatrix,
                               GetJitteredViewProj(m_pCamera, m_displaySize),
                               passID);
        m_pMaterial->SetFloat4(ShaderIDs::g_ScreenSize,
                               GetScreenSize(pickWidth, pickHeight),
                               passID);
        m_pMaterial->SetUINT(ShaderIDs::g_DepthTexIndex,
                             m_pCameraDepthRT->GetSRVResourceHeapIndex(),
                             passID);

        // One draw per item; the per-item world matrix and the render list index
        // (the id written into the pick RT) are re-uploaded per draw.
        for (UINT drawIndex = 0; drawIndex < pickDrawItems.size(); ++drawIndex)
        {
            RenderItem* pItem = pickDrawItems[drawIndex];
            m_pMaterial->SetMatrix(ShaderIDs::g_WorldMatrix, pItem->worldMatrix, passID);
            m_pMaterial->SetUINT(ShaderIDs::g_EntityID, pickDrawIds[drawIndex], passID);
            SetSpaceResource(passData, PER_PASS_SPACE);

            m_pCommand->DrawInstanced(pItem->indexCount, 1, pItem->startIndex,
                                      pItem->baseVertex, 0);
        }

        // Copy the single clicked pixel into the readback buffer.
        // CopyTextureRegion needs a 256-byte aligned row pitch, hence the 1x1
        // footprint and the 256-byte readback buffer.
        const INT pixelX = std::clamp(
            static_cast<INT>(viewportUV.x * static_cast<float>(pickWidth)), 0,
            static_cast<INT>(pickWidth - 1));
        const INT pixelY = std::clamp(
            static_cast<INT>(viewportUV.y * static_cast<float>(pickHeight)), 0,
            static_cast<INT>(pickHeight - 1));

        D3D12_TEXTURE_COPY_LOCATION dst{};
        dst.pResource = m_pPickReadback->GetResource().Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dst.PlacedFootprint.Offset = 0;
        dst.PlacedFootprint.Footprint.Format = PickRTFormat;
        dst.PlacedFootprint.Footprint.Width = 1;
        dst.PlacedFootprint.Footprint.Height = 1;
        dst.PlacedFootprint.Footprint.Depth = 1;
        dst.PlacedFootprint.Footprint.RowPitch = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;

        D3D12_TEXTURE_COPY_LOCATION src{};
        src.pResource = m_pPickRT->GetTexture()->GetResource().Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.SubresourceIndex = 0;

        const D3D12_BOX box
        {
            static_cast<UINT>(pixelX),
            static_cast<UINT>(pixelY),
            0,
            static_cast<UINT>(pixelX + 1),
            static_cast<UINT>(pixelY + 1),
            1
        };
        m_pCommand->AddBarrier(m_pPickRT, D3D12_RESOURCE_STATE_COPY_SOURCE);
        m_pCommand->GetCommandList()->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);

        // Leave the pick RT readable (debug view samples it) and hand the depth
        // back in the state the engine's compute passes expect.
        m_pCommand->AddBarrier(m_pPickRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_pCommand->AddBarrier(m_pCameraDepthRT,
                               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
                               D3D12_RESOURCE_STATE_DEPTH_READ);

        m_bPickResolvePending = true;
    }

    void SelectionOutlinePass::ResolvePendingPick(const std::vector<RenderItem>& currentRenderList)
    {
        if (!m_bPickResolvePending)
            return;

        m_bPickResolvePending = false;

        if (!m_pPickReadback)
            return;

        // The copy recorded on the previous frame is submitted, but the CPU can
        // run a frame or two ahead of the GPU: mapping right away would read the
        // untouched buffer (all zeros) and every click would resolve to
        // "background". Drain the queues first - this only costs a one-off sync
        // on the frame that resolves a click.
        m_pDevice->WaitForIdle();

        UINT* pMappedData = nullptr;
        const D3D12_RANGE readRange{0, sizeof(UINT)};
        const HRESULT hr = m_pPickReadback->GetResource()->Map(
            0,
            &readRange,
            reinterpret_cast<void**>(&pMappedData));

        if (SUCCEEDED(hr) && pMappedData)
        {
            float pickValueF = 0.f;
            std::memcpy(&pickValueF, pMappedData, sizeof(pickValueF));
            const UINT pickValue = static_cast<UINT>(pickValueF);
            const D3D12_RANGE writeRange{0, 0};
            m_pPickReadback->GetResource()->Unmap(0, &writeRange);

            // value = render list index + 1; 0 = background. Validate the pointer
            // against the current frame's list, so a scene reload in between
            // cannot hand us a dangling Entity.
            ElysiaEngine::Entity* pHitEntity = nullptr;
            if (pickValue > 0 && pickValue - 1 < m_PickEntityLUT.size())
            {
                ElysiaEngine::Entity* pCandidate = m_PickEntityLUT[pickValue - 1];
                const bool bStillAlive = std::any_of(
                    currentRenderList.begin(), currentRenderList.end(),
                    [pCandidate](const RenderItem& ri)
                    {
                        return ri.pAssociatedEntity == pCandidate;
                    });
                if (bStillAlive)
                {
                    pHitEntity = pCandidate;
                }
            }

            SelectionManager::GetInstance().ResolvePick(pHitEntity);
        }
        else
        {
            ElysiaHelper::Log::Warn("[Selection] pick readback Map failed (hr=0x%08X)", hr);
        }

        m_PickEntityLUT.clear();
    }

    void SelectionOutlinePass::UpdatePipeline()
    {
        if (!m_pMaterial || !m_pMaskRT || !m_pPickRT || !m_pCameraDepthRT)
            return;

        auto updatePassVariant = [this](UINT passID, DXGI_FORMAT renderTargetFormat)
        {
            auto& passData = m_pMaterial->GetPassData(passID);
            auto* pVariantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &pVariantManager->GetOrCompileVariantByNames({});

            const RenderTargetDesc desc
            {
                .m_renderTargetFormats = {renderTargetFormat},
                .m_numRenderTargets = 1,
                // Mask/Pick shaders do the depth comparison manually
                // (Depth Disabled), no depth target is bound
                .m_depthStencilFormat = m_pCameraDepthRT->GetFormat(),
            };
            passData.pPipelineStateObject = PSOManager::GetInstance().GetGraphicsPipelineState(
                m_pDevice,
                m_pMaterial.get(),
                passID,
                desc,
                D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

            // A null PSO (e.g. a PS output type / render target format mismatch)
            // silently draws nothing, so surface it instead of guessing later.
            if (!passData.pPipelineStateObject)
            {
                ElysiaHelper::Log::Error(
                    "[SelectionOutline] PSO creation failed for pass %u (format=%u)",
                    passID, static_cast<UINT>(renderTargetFormat));
            }
        };

        updatePassVariant(ShaderPassIDs::MaskPassID, MaskRTFormat);
        updatePassVariant(ShaderPassIDs::OutlinePassID, m_pDisplayRT->GetFormat());
        updatePassVariant(ShaderPassIDs::PickPassID, PickRTFormat);
    }
}
