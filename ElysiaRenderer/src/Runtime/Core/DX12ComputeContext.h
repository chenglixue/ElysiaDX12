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
		void Dispatch(size_t groupCountX, size_t groupCountY, size_t groupCountZ);
		void Dispatch1D(size_t threadCountX, size_t groupSizeX);
		void Dispatch2D(size_t threadCountX, size_t threadCountY, size_t groupSizeX, size_t groupSizeY);
		void Dispatch3D(size_t threadCountX, size_t threadCountY, size_t threadCountZ, size_t groupSizeX, size_t groupSizeY, size_t groupSizeZ);

	private:
		PipelineStateObject* m_pCurrentPipeline = nullptr;
	};
}