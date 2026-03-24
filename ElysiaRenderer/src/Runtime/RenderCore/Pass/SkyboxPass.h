#pragma once
#include "BasePass.h"
#include "Runtime/RenderCore/RenderResource.h"

namespace ElysiaRenderer
{
    class RenderTexture;

#define SKYBOX_PASS_LIST \
    PASS(Draw_SKY_BOX_PASS, "public\\Skybox.hlsl")

    class SkyboxPass : public BasePass
    {
    public:
        SkyboxPass();
        virtual ~SkyboxPass() override;

        virtual void Configure() override;
        virtual void Render(ElysiaEngine::FrameContext& frameContext) override;
        virtual void UpdatePipeline() override;

        virtual void Dispose() override;

    private:
        struct ShaderIDs
        {
            static inline size_t screenSize = PropertyToID(L"screenSize");
            static inline size_t viewMatrix = PropertyToID(L"viewMatrix");
            static inline size_t viewMatrix_I = PropertyToID(L"viewMatrix_I");
            static inline size_t projMatrix = PropertyToID(L"projMatrix");
            static inline size_t projMatrix_I = PropertyToID(L"projMatrix_I");
            static inline size_t viewProjMatrix = PropertyToID(L"viewProjMatrix");
            static inline size_t viewProjMatrix_I = PropertyToID(L"viewProjMatrix_I");
            static inline size_t worldMatrix = PropertyToID(L"worldMatrix");
        };

#pragma region Pass
        enum PassID
        {
#define PASS(id, file) id,
            SKYBOX_PASS_LIST
#undef PASS
            SKYBOX_PASS_COUNT
        };
        static inline const ShaderPass m_PassData[] =
        {
#define PASS(id, file) \
{ \
.Name = #id, \
.FilePath = L"Shaders\\" L##file, \
},
            SKYBOX_PASS_LIST
#undef PASS
        };
#pragma endregion


        static constexpr uint64 NumVertices = 8;
        static constexpr uint64 NumIndices = 36;
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
            0, 1, 2, 2, 3, 0, // Front
            1, 4, 7, 7, 2, 1, // Right
            4, 5, 6, 6, 7, 4, // Back
            5, 0, 3, 3, 6, 5, // Left
            5, 4, 1, 1, 0, 5, // Top
            3, 2, 7, 7, 6, 3  // Bottom
        };
#pragma endregion
        UINT m_cameraWidth;
        UINT m_cameraHeight;
        BufferHandle m_vertexBuffer;
        BufferHandle m_indexBuffer;
        D3D12_VERTEX_BUFFER_VIEW m_vertexView;
        D3D12_INDEX_BUFFER_VIEW m_indexView;

        void DrawSkybox();
    };
}