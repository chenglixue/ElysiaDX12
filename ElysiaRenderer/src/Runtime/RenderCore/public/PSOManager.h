#pragma once
#include "Programs/public/Helper.h"
#include "Programs/public/IManager.h"
#include "Runtime/Core/public/PipelineStateUtility.h"
#include "Runtime/Core/public/DX12PipelineState.h"

#include <functional>

namespace ElysiaCore
{
    class DX12Device;
}

namespace ElysiaRenderer
{
    class Material;
}

namespace ElysiaRenderer
{
    class PSOManager : IManager
    {
    public:
        PSOManager() = default;
        PSOManager(const PSOManager& rhs) = delete;
        PSOManager& operator=(PSOManager& rhs) = delete;
        PSOManager(PSOManager&& rhs) = default;
        ~PSOManager();

        static PSOManager& GetInstance()
        {
            std::call_once(m_initInstanceFlag,
                           []()
                           {
                               m_instance.reset(new PSOManager());
                           });

            return *m_instance;
        }

        virtual void Init(DX12Device* pDevice) override;
        virtual void Destory() override;

        // Bracket pass setup. Pipeline creation requested inside the batch
        // runs on the precache threads; Wait blocks until that batch is done.
        void BeginPrecacheBatch();
        void WaitPrecacheBatch();
        void EnqueuePrecacheWork(std::function<void()> work, const char* name);

        PipelineStateObject* GetGraphicsPipelineState(DX12Device* pDevice,
                                                      Material* pMaterial,
                                                      UINT passIndex,
                                                      const RenderTargetDesc& renderTargetDesc,
                                                      D3D12_PRIMITIVE_TOPOLOGY_TYPE topology =
                                                          D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

        PipelineStateObject* GetComputePipelineState(DX12Device* pDevice,
                                                     Material* pMaterial,
                                                     UINT passIndex);

        PipelineStateObject* GetGraphicsPipelineState(DX12Device* pDevice,
                                                      const D3D12_GRAPHICS_PIPELINE_STATE_DESC&
                                                      PSODesc,
                                                      DX12RootSignature* pRootSignature);
        PipelineStateObject* GetComputePipelineState(DX12Device* pDevice,
                                                     const D3D12_COMPUTE_PIPELINE_STATE_DESC&
                                                     PSODesc,
                                                     DX12RootSignature* pRootSignature);

    private:
        DX12Device* m_pDevice = nullptr;
        static std::unique_ptr<PSOManager> m_instance;
        static std::once_flag m_initInstanceFlag;
    };
}