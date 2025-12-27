#pragma once
#include "DX12Context.h"

namespace ElysiaCore
{
	class DX12Device;
	class PipelineStateObject;
	class PipelineResourceSpace;
	struct PipelineInfo;
}

namespace ElysiaCore
{
	class DX12ComputeContext : public DX12Context
	{
	public:
		DX12ComputeContext(DX12Device* device);
		~DX12ComputeContext() override;

		void SetPipeline(PipelineInfo& pipelineStateData);
		void SetPipelineResource(uint8_t spaceID, PipelineResourceSpace* pipelineBindResource);
		void Dispatch(UINT groupCountX, UINT groupCountY, UINT groupCountZ);
		void Dispatch1D(UINT threadCountX, UINT groupSizeX);
		void Dispatch2D(UINT threadCountX, UINT threadCountY, UINT groupSizeX, UINT groupSizeY);
		void Dispatch3D(UINT threadCountX, UINT threadCountY, UINT threadCountZ, UINT groupSizeX, UINT groupSizeY, UINT groupSizeZ);

	private:
		PipelineStateObject* m_pCurrentPipeline = nullptr;
	};
}