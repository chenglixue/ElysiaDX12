#pragma once
#include "PipelineStateUtility.h"

namespace ElysiaRenderer
{
	extern class DX12RootSignature;
	extern class DX12Shader;

	class DX12PipelineState
	{
	public:
		DX12PipelineState();
		DX12PipelineState(CComPtr<ID3D12PipelineState> pipelineState);
		DX12PipelineState(CComPtr<ID3D12PipelineState> pipelineState, DX12RootSignature* rootSignature);
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
		DX12GraphicsPipelineState(CComPtr<ID3D12PipelineState> pipelineState, DX12RootSignature* rootSignature);
		~DX12GraphicsPipelineState() override;

	private:
	};

	
}