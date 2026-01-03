#include "stdafx.h"
#include "UIPass.h"

#include "Runtime/Core/DX12Device.h"
#include"Editor/IMGUIHelper.h"
#include "Editor/UserData.h"
#include "Programs/PIXHelper.h"
#include "Runtime/Core/DX12GraphicsContext.h"

#include "Runtime/RenderCore/RenderTexture.h"
#include "Runtime/RenderCore/RenderResource.h"

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
            context.buildUI();
        }
    }

    void UIPass::Dispose()
    {

    }
}