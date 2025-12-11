#pragma once
#include "BasePass.h"
#include "lib/DX12/DX12Shadow.h"
#include "Manager/LightManager.h"
#include "lib/Utility/RenderTexture.h"

namespace ElysiaRenderer
{
    class PreDrawPass : public BasePass
    {
    public:
        PreDrawPass(DX12Camera* pCamera);
        virtual ~PreDrawPass() override;

        //virtual void Setup(const RenderPassData& renderPasssData) override;
        virtual void Configure() override;
        virtual void Execute() override;
        virtual void Render() override;
        virtual void UpdatePSO() override;
        virtual void UpdateVariant() override;

        virtual void Dispose() override;
    };
}

