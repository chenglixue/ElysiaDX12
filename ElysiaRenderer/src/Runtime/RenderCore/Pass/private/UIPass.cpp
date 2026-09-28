#include "stdafx.h"
#include "../public/UIPass.h"

#include "Runtime/Core/public/DX12Device.h"
#include"Editor/public/IMGUIHelper.h"
#include "Editor/public/UserData.h"
#include "Programs/public/PIXHelper.h"
#include "Runtime/Core/public/DX12GraphicsContext.h"

#include "Runtime/RenderCore/public/RenderTexture.h"
#include "Runtime/RenderCore/public/RenderResource.h"

namespace ElysiaRenderer
{
    UIPass::UIPass()
    {

    }
    UIPass::~UIPass()
    {
        Dispose();
    }

    void UIPass::Configure()
    {

    }
    void UIPass::Render(ElysiaEngine::FrameContext& context)
    {
        PIXHelper pix(m_pCommand->GetCommandList(), "UI Pass");

        if (context.buildUI)
        {
            m_pCommand->AddBarrier(m_pDisplayRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            context.buildUI();
            m_pCommand->AddBarrier(m_pDisplayRT,
                                   D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        }
    }

    void UIPass::Dispose()
    {

    }
}