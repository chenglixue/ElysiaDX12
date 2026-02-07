#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/RenderResource.h"

namespace ElysiaRenderer
{
    class RenderTexture;
}

namespace ElysiaRenderer
{
    using namespace ElysiaEngine;
    using namespace CAULDRON_DX12;
    using namespace ElysiaHelper;

    class DebugPass : public BasePass
    {
    public:
        DebugPass();
        virtual ~DebugPass() override;

        virtual void Configure() override;
        virtual void Render(FrameContext& context) override;
        virtual void Dispose() override;
        virtual void UpdatePipeline() override;

    private:
        struct ShaderPasseIDs
        {
            static inline int DebugPassID = -1;
        };

        struct ShaderIDs
        {
            static inline size_t g_DebugMode = PropertyToID(L"g_DebugMode");
            static inline size_t g_TargetTexIndex = PropertyToID(L"g_TargetTexIndex");
            static inline size_t g_SourceTexIndex = PropertyToID(L"g_SourceTexIndex");
            static inline size_t g_MipmapLevel = PropertyToID(L"g_MipmapLevel");
            static inline size_t g_SourceSize = PropertyToID(L"g_SourceSize");
            static inline size_t g_TargetSize = PropertyToID(L"g_TargetSize");
            static inline size_t screenSize = PropertyToID(L"screenSize");
            static inline size_t viewMatrix = PropertyToID(L"viewMatrix");
            static inline size_t viewMatrix_I = PropertyToID(L"viewMatrix_I");
            static inline size_t projMatrix = PropertyToID(L"projMatrix");
            static inline size_t projMatrix_I = PropertyToID(L"projMatrix_I");
            static inline size_t viewProjMatrix = PropertyToID(L"viewProjMatrix");
            static inline size_t viewProjMatrix_I = PropertyToID(L"viewProjMatrix_I");
        };

        static constexpr uint64 NumVertices = 8;
        static constexpr uint64 NumIndices = 24;
#pragma region Vertices
        const Vector3 m_vertices[NumVertices] =
        {
            Vector3(-1.0f, 1.0f, 1.0f),
            Vector3(1.0f, 1.0f, 1.0f),
            Vector3(1.0f, -1.0f, 1.0f),
            Vector3(-1.0f, -1.0f, 1.0f),
            Vector3(1.0f, 1.0f, -1.0f),
            Vector3(-1.0f, 1.0f, -1.0f),
            Vector3(-1.0f, -1.0f, -1.0f),
            Vector3(1.0f, -1.0f, -1.0f),
        };
#pragma endregion
#pragma region Indices
        static constexpr INDEX_FORMAT m_indices[NumIndices] =
        {
            0, 1, 1, 2, 2, 3, 3, 0, // 前面四条边
            4, 5, 5, 6, 6, 7, 7, 4, // 后面四条边
            0, 5, 1, 4, 2, 7, 3, 6
        };
#pragma endregion
        BufferHandle m_vertexBuffer;
        BufferHandle m_indexBuffer;
        D3D12_VERTEX_BUFFER_VIEW m_vertexView;
        D3D12_INDEX_BUFFER_VIEW m_indexView;

        void DoDebugPass();
    };
}