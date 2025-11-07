#pragma once
#include "Helper.h"
#include "IManager.h"

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

		virtual void Init() override;
		virtual void Destory() override;

		ID3D12PipelineState* GetPipelineState(D3D12_GRAPHICS_PIPELINE_STATE_DESC const& stateDesc);

	private:
		std::unordered_map<D3D12_GRAPHICS_PIPELINE_STATE_DESC, CComPtr<ID3D12PipelineState>> m_pipelineStates;
	};
}