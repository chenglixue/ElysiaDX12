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

    protected:
        struct ShaderPasseIDs
        {
            static inline int DebugPassID = -1;
        };
        struct ShaderIDs
        {
            static inline size_t g_DebugMode = PropertyToID(L"g_DebugMode");
            static inline size_t g_TargetTexIndex = PropertyToID(L"g_TargetTexIndex");
            static inline size_t g_SourceTexIndex = PropertyToID(L"g_SourceTexIndex");
            static inline size_t g_AABBInstanceDatasIndex = PropertyToID(
                L"g_AABBInstanceDatasIndex");
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

        struct AABBDrawer
        {
            struct alignas(16) AABBInstanceData
            {
                Vector3 Min;
                float pad0;
                Vector3 Max;
                float pad1;
                Color Color;
            };

            std::vector<AABBInstanceData> m_instanceCpuData;

            static constexpr uint64 NumVertices = 8;
            static constexpr uint64 NumIndices = 24;
#pragma region Vertices
            const Vector3 vertices[NumVertices] =
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
            static constexpr INDEX_FORMAT indices[NumIndices] =
            {
                0, 1, 1, 2, 2, 3, 3, 0, // 前面四条边
                4, 5, 5, 6, 6, 7, 7, 4, // 后面四条边
                0, 5, 1, 4, 2, 7, 3, 6
            };
#pragma endregion

            D3D12_VERTEX_BUFFER_VIEW vertexView;
            D3D12_INDEX_BUFFER_VIEW indexView;

            void Init();
            void AddAABB(const Vector3& min, const Vector3& max, const Vector4& color);
            void AddAABB(const Vector3& min, const Vector3& max, const UINT index);
            void Clear();
            void Bind(Material* pMaterail);
            void Draw(DX12GraphicsContext* context);
            bool IsReady() const noexcept
            {
                return vertexBuffer->GetIsReady() && indexBuffer->GetIsReady();
            }

        private:
            BufferHandle vertexBuffer;
            BufferHandle indexBuffer;
            BufferHandle instanceDataBuffer;
            void UploadVertexIndex();
            Color GetDebugColor(uint32_t id);
        };

        AABBDrawer m_aabbDrawer;

        void DoDebugPass();
        void DoAABBPass();
        void DoGIPass();
    };
}