#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/RenderResource.h"

namespace ElysiaRenderer
{
    class DX12Light;
}

namespace ElysiaRenderer
{
    using namespace ElysiaHelper;

    class ShadowPass : public BasePass
    {
    public:
        struct ShaderPassIDs
        {
            static inline int ShadowCastPassID = -1;
        };
        struct RenderTextureIDs
        {
            static inline size_t ShadowRTID = PropertyToID(L"Shadow RT");
        };
        struct ShaderIDs
        {
            static inline size_t shadowNearZ = PropertyToID(L"shadowNearZ");
            static inline size_t shadowFarZ = PropertyToID(L"shadowFarZ");
            static inline size_t shadowDepthBias = PropertyToID(L"shadowDepthBias");
            static inline size_t shadowSlopeDepthBias = PropertyToID(L"shadowSlopeDepthBias");
            static inline size_t shadowMaxSlopeDepthBias = PropertyToID(L"shadowMaxSlopeDepthBias");
            static inline size_t g_sobolSequence = PropertyToID(L"g_sobolSequence");
            static inline size_t worldMatrix = PropertyToID(L"worldMatrix");
            static inline size_t baseColorTexIndex = PropertyToID(L"baseColorTexIndex");
            static inline size_t opacity = PropertyToID(L"opacity");
            static inline size_t cutoff = PropertyToID(L"cutoff");
        };

    public:
        ShadowPass();
        virtual ~ShadowPass() override;

        virtual void Configure() override;
        virtual void Render(ElysiaEngine::FrameContext& context) override;
        virtual void UpdatePipeline() override;

        virtual void Dispose() override;

    private:
        static constexpr auto Max_RenderItem_Count = 1024;

        struct alignas(16) MeshData
        {
            Matrix world_M;

            float opacity;
            float cutoff;
            UINT baseColorTexIndex;
            UINT vertexOffset;

            UINT indexOffset;
            UINT3 pad;
        };
        struct alignas(16) IndirectCommand
        {
            struct
            {
                UINT meshDataBufferIndex;
                UINT meshDataIndex;
            } pushConstants;

            D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
        };

        DX12Light* m_pMainLight;
        std::vector<Vector2> m_sobolSqeuences;
        std::vector<MeshData> m_meshDatas;
        std::vector<IndirectCommand> m_indirectCommands;
        BufferHandle m_pMeshDataBuffer;
        BufferHandle m_pIndirectDataBuffer;
        CComPtr<ID3D12CommandSignature> m_pCommandSignature;

        void UpdateShadowPassVariant(UINT passIndex);
        void DrawMesh(ElysiaEngine::FrameContext& context, PassData& passData);
        void DrawShadowPass(ElysiaEngine::FrameContext& context);
        void UploadMeshData(const std::vector<RenderItem>& renderItems);
    };
}