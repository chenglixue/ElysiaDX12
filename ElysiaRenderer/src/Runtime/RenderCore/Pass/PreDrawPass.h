#pragma once
#include "BasePass.h"

namespace ElysiaRenderer
{
    class PreDrawPass : public BasePass
    {
    public:
        PreDrawPass();
        virtual ~PreDrawPass() override;

        //virtual void Setup(const RenderPassData& renderPasssData) override;
        virtual void Configure() override;
        virtual void Render(ElysiaEngine::FrameContext& context) override;
        virtual void UpdatePipeline() override;

        virtual void Dispose() override;
    };
}