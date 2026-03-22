#include "stdafx.h"
#include "TAAPass.h"

#include "GBufferPass.h"
#include "../TAAUtility.h"

#include "Editor/UserData.h"
#include "Programs/PIXHelper.h"
#include "Editor/IMGUIHelper.h"

#include "Runtime/Core/DX12GraphicsContext.h"
#include "Runtime/Core/DX12Shader.h"
#include "Runtime/Core/DX12Device.h"
#include "Runtime/Core/DX12RenderPassDescriptorHeap.h"
#include "Runtime/Core/DX12TextureBuffer.h"
#include "Runtime/Core/SwapChain.h"
#include "Runtime/RenderCore/CameraManager.h"

#include "Runtime/RenderCore/RenderResource.h"
#include "Runtime/RenderCore/RenderTexture.h"
#include "Runtime/RenderCore/Material.h"
#include "Runtime/RenderCore/PSOManager.h"
#include "Runtime/RenderCore/RenderTargetManager.h"
#include "Runtime/RenderCore/ShaderVariantManager.h"

namespace ElysiaRenderer
{
    TAAPass::TAAPass()
    {

    }
    TAAPass::~TAAPass()
    {
        Dispose();
    }
    void TAAPass::Dispose()
    {

    }

    void TAAPass::Configure()
    {
        m_cameraWidth = m_renderSize.x;
        m_cameraHeight = m_renderSize.y;
    }
    void TAAPass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        for (UINT i = 0; i < TAA_PASS_COUNT; ++i)
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
    }
    void TAAPass::Render(FrameContext& context)
    {
        if (!UserData::GetInstance().bloomParameter.enable)
            return;
        PIXHelper pix(m_pCommand->GetCommandList(), "TAA Pass");
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), "TAA Begin");

    }
}