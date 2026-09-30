#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/public/RenderResource.h"

namespace ElysiaRenderer
{
    class RenderTexture;

    // Viewport selection rendering:
    //   Mask + Outline : editor outline around the selected entity
    //   Pick           : UE-style HitProxy id buffer used to resolve viewport clicks
    class SelectionOutlinePass : public BasePass
    {
    public:
        SelectionOutlinePass();
        virtual ~SelectionOutlinePass() override;

        virtual void Configure() override;
        virtual void Render(ElysiaEngine::FrameContext& context) override;
        virtual void UpdatePipeline() override;
        virtual void Dispose() override;

    private:
        struct ShaderPassIDs
        {
            static inline int MaskPassID = -1;
            static inline int OutlinePassID = -1;
            static inline int PickPassID = -1;
        };

        struct ShaderIDs
        {
            static inline size_t g_WorldMatrix = PropertyToID(L"g_WorldMatrix");
            static inline size_t g_ViewProjMatrix = PropertyToID(L"g_ViewProjMatrix");
            static inline size_t g_ScreenSize = PropertyToID(L"g_ScreenSize");
            static inline size_t g_DepthTexIndex = PropertyToID(L"g_DepthTexIndex");
            static inline size_t g_EntityID = PropertyToID(L"g_EntityID");
            static inline size_t g_MaskSize = PropertyToID(L"g_MaskSize");
            static inline size_t g_MaskTexIndex = PropertyToID(L"g_MaskTexIndex");
        };

        // Selected entity silhouette; R = visible surface, G = full coverage
        RenderTexture* m_pMaskRT = nullptr;

        // HitProxy pick target (R32_FLOAT holding render item index + 1) plus the
        // 1x1 pixel readback used to resolve the click on the following frame
        RenderTexture* m_pPickRT = nullptr;
        BufferHandle m_pPickReadback;
        bool m_bPickResolvePending = false;
        std::vector<ElysiaEngine::Entity*> m_PickEntityLUT;

        void DrawMask(ElysiaEngine::FrameContext& context, ElysiaEngine::Entity* pSelected);
        void DrawOutline(ElysiaEngine::FrameContext& context, ElysiaEngine::Entity* pSelected);
        void RenderPick(ElysiaEngine::FrameContext& context, const Vector2& viewportUV);
        void ResolvePendingPick(const std::vector<RenderItem>& currentRenderList);
    };
}
