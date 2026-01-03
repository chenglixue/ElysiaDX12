#pragma once
#include "PipelineStateUtility.h"
#include "PipelineResourceUtility.h"

namespace ElysiaCore
{
    class DX12RootSignature;
}

namespace ElysiaCore
{
    class DX12PipelineState
    {
    public:
        DX12PipelineState();
        DX12PipelineState(CComPtr<ID3D12PipelineState> pipelineState);
        DX12PipelineState(CComPtr<ID3D12PipelineState> pipelineState,
                          DX12RootSignature* rootSignature);
        virtual ~DX12PipelineState();

        CComPtr<ID3D12PipelineState> GetPipelineState();
        DX12RootSignature*& GetRootSignature();
        PipelineType GetPipelineType();

    protected:
        CComPtr<ID3D12PipelineState> m_pipelineState;
        PipelineType m_pipelineType;
        DX12RootSignature* m_rootSignature;
    };

    class DX12GraphicsPipelineState : public DX12PipelineState
    {
    public:
        DX12GraphicsPipelineState();
        DX12GraphicsPipelineState(CComPtr<ID3D12PipelineState> pipelineState,
                                  DX12RootSignature* rootSignature);
        ~DX12GraphicsPipelineState() override;

    private:
    };

    class DX12ComputePipelineState : public DX12PipelineState
    {
    public:
        DX12ComputePipelineState();
        DX12ComputePipelineState(CComPtr<ID3D12PipelineState> pipelineState,
                                 DX12RootSignature* rootSignature);
        ~DX12ComputePipelineState() override;

    private:
    };

    struct PipelineStateObject
    {
        PipelineStateObject();
        ~PipelineStateObject();

        std::unique_ptr<DX12PipelineState> m_pipelineState = nullptr;
        PipelineResourceMapping m_pipelineResourceMapping;
        DX12RootSignature* m_rootSignature = nullptr;
        PipelineType m_pipelineType = PipelineType::Graphics;
    };

    struct PipelineInfo
    {
        PipelineStateObject* m_pipelineStateObject = nullptr;
        std::vector<DX12TextureResource*> m_renderTargets{};
        DX12TextureResource* m_depthStencilTarget = nullptr;
    };
}