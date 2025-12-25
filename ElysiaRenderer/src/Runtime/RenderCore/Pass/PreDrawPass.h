#pragma once
#include "BasePass.h"

namespace ElysiaRenderer
{
    class PreDrawPass : public BasePass
    {
    public:
        PreDrawPass(DX12Camera* pCamera);
        virtual ~PreDrawPass() override;

        //virtual void Setup(const RenderPassData& renderPasssData) override;
        virtual void Configure() override;
        virtual void Render(ElysiaEngine::FrameContext context) override;
        virtual void UpdatePSO() override;
        virtual void UpdateVariant() override;

        virtual void Dispose() override;
    };
}

