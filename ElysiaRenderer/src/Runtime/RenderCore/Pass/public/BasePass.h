#pragma once
#include "Runtime/Core/public/DX12PipelineState.h"
#include "Runtime/Core/public/ShaderUtility.h"
#include "Runtime/Engine/public/FrameContext.h"

// One list expands to enum PassID + ShaderPass table.
// Usage:
//   #define FOO_PASS_LIST(X) \
//       X(MyPass, "public\\Foo.hlsl", true, Main)
//   class FooPass : public BasePass {
//       DECLARE_SHADER_PASSES(FOO_PASS_LIST, FOO_PASS_COUNT)
//   };
#define SHADER_PASS_ENUM(id, ...) id,
#define SHADER_PASS_DATA(id, file, isCS, entry) \
    { \
        .Name = #id, \
        .FilePath = L"Shaders\\" L##file, \
        .IsComputeShader = isCS, \
        .ComputeEntryPoint = L#entry \
    },
#define DECLARE_SHADER_PASSES(LIST, COUNT) \
    enum PassID \
    { \
        LIST(SHADER_PASS_ENUM) \
        COUNT \
    }; \
    static inline const ShaderPass PassData[] = \
    { \
        LIST(SHADER_PASS_DATA) \
    }; \
    static_assert(sizeof(PassData) / sizeof(PassData[0]) == COUNT)

namespace ElysiaCore
{
    class DX12GraphicsContext;
    class DX12Device;
    class UploadRingBuffer;
    struct PipelineStateObject;
    class SwapChain;
}

namespace ElysiaRenderer
{
    class RenderTexture;
    class DX12Camera;
    class Material;
    struct PassData;
    struct RenderPassData;
}

namespace ElysiaRenderer
{
    using namespace ElysiaCore;
    using namespace ElysiaRenderer;

    class BasePass
    {
    public:
        BasePass();
        virtual ~BasePass();

        virtual void Setup(const RenderPassData& renderPassData);
        virtual void Configure() = 0;
        virtual void Render(ElysiaEngine::FrameContext& context) = 0;

        virtual void Dispose();

        virtual void UpdatePipeline() = 0;

        // Called when the user changes the shadow map resolution. Passes that own
        // shadow-sized resources rebuild them here, instead of going through the
        // full window-size-dependent rebuild (which recreates every resource and
        // re-runs the whole PSO precache batch).
        virtual void OnShadowResolutionChanged()
        {
        }

        D3D12_GPU_VIRTUAL_ADDRESS UploadMaterialConstants(
            UploadRingBuffer* pUploadBuffer,
            UINT8 spaceID,
            Material* pMaterial,
            const ShaderVariantData* pVariantData,
            size_t passID = 0);

    protected:
        Vector2 m_displaySize;
        UINT m_frameIndex;

        DX12Device* m_pDevice = nullptr;
        DX12GraphicsContext* m_pCommand = nullptr;
        SwapChain* m_pSwaiChain = nullptr;
        ElysiaHelper::GPUTimestamps* m_pGPUTimer = nullptr;
        DX12Camera* m_pCamera = nullptr;
        RenderTexture* m_pCameraColorRT = nullptr;
        RenderTexture* m_pCameraDepthRT = nullptr;
        RenderTexture* m_pDisplayRT = nullptr;

        std::vector<ShaderPass> m_shaderPasses;
        std::unique_ptr<Material> m_pMaterial = nullptr;

        RenderTexture* m_pWarmUPRT = nullptr;

        void SetSpaceResource(PassData& passData, UINT8 spaceID);
        void WarmUPCompute();

    private:
        std::unique_ptr<Material> m_pWarmUPMaterial = nullptr;
        std::vector<ShaderPass> m_WarmUPShaderPasses;
        int m_warmUpGraphicsPasseID = -1;
        int m_warmUpComputePasseID = -1;
    };
}