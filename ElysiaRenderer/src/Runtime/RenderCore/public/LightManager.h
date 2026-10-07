#pragma once

#include "Programs/public/IManager.h"
#include "Programs/public/IUpdate.h"
#include "Runtime/RenderCore/public/ShadowUtility.h"

namespace ElysiaRenderer
{
    class DX12DirectionLight;
    class RenderTexture;
    class DX12Shadow;
}

namespace ElysiaEngine
{
    struct Entity;
}

namespace ElysiaRenderer
{
    using namespace ElysiaEngine;

    class LightManager : public IManager, IUpdate
    {
    public:
        LightManager();
        LightManager(const LightManager& rhs) = delete;
        LightManager& operator=(LightManager& rhs) = delete;
        LightManager(LightManager&& rhs) = default;
        ~LightManager();

        static LightManager& GetInstance()
        {
            std::call_once(m_initInstanceFlag,
                           []()
                           {
                               m_instance.reset(new LightManager());
                           });

            return *m_instance;
        }

        virtual void Init(ElysiaCore::DX12Device* pDevice) override;
        virtual void Destory() override;
        virtual void Update(const ElysiaEngine::FrameContext& context) override;

        DX12DirectionLight* GetMainLight();
        DX12Shadow* GetMainShadow();
        RenderTexture* GetMainShadowRT() const;
        bool ConsumeShadowLayoutDirty();

    private:
        static std::unique_ptr<LightManager> m_instance;
        static std::once_flag m_initInstanceFlag;
        ElysiaCore::DX12Device* m_pDevice = nullptr;

        UINT m_frameID;
        UINT64 m_frameIndex;

        std::unique_ptr<DX12DirectionLight> m_pMainLight = nullptr;
        ShadowType m_appliedShadowType = ShadowType::Soft;
        ShadowQuality m_appliedShadowQuality = ShadowQuality::VeryHigh;
        bool m_bShadowLayoutDirty = false;

        void CreatMainLight();
        void CaptureAppliedShadowLayout();
        void DetectShadowLayoutChange();
        void AppendCasterBounds(Entity& entity, std::vector<BoundingBox>& casterBounds);
    };
}