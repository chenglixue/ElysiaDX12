#pragma once
#include "Helper.h"

namespace ElysiaRenderer
{
	using namespace ElysiaHelper;

	class DX12GPUResource;
	class PipelineResourceSpace;

	struct PipelineResourceMapping
	{
		// space id : root paramter index
		std::array<std::optional<UINT>, NUM_RESOURCE_SPACES> m_CBVMappings{};
		std::array<std::optional<UINT>, NUM_RESOURCE_SPACES> m_TableMappings{};
	};

	struct PipelineResourceBinding
	{
		UINT m_bindingIndex = 0;
		DX12GPUResource* m_resource = nullptr;
	};

	struct PipelineResourceLayout
	{
		std::array<PipelineResourceSpace*, NUM_RESOURCE_SPACES> m_spaces{  };
	};

	inline bool SortPipelineBindings(PipelineResourceBinding* a, PipelineResourceBinding* b)
	{
		return (*a).m_bindingIndex < (*b).m_bindingIndex;
	}
}