#pragma once
#include "stdafx.h"
#include "DX12TextureResource.h"

namespace ElysiaRenderer
{
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

	/// <summary>
	/// save all resource in root parameters
	/// </summary>
	class PipelineResourceSpace
	{
	public:
		PipelineResourceSpace() = default;
		PipelineResourceSpace(const PipelineResourceSpace& rhs) = default;
		PipelineResourceSpace& operator=(const PipelineResourceSpace& rhs) = default;
		PipelineResourceSpace(PipelineResourceSpace&& rhs) = default;
		~PipelineResourceSpace() = default;

		DX12BufferResource* GetCBV()
		{
			return m_CBV;
		}
		std::vector<PipelineResourceBinding*>& GetSRVs()
		{
			return m_SRVs;
		}

		void SetCBV(DX12BufferResource* CBVResource);
		void SetSRV(PipelineResourceBinding* SRVResource);

		void Lock()
		{
			m_isLocked = true;
		}
		bool IsLocked() const
		{
			return m_isLocked;
		}

	private:
		UINT GetIndexOfBindingIndex(const std::vector<PipelineResourceBinding*>& bindResources, UINT bindingIndex);

		DX12BufferResource* m_CBV;
		std::vector<PipelineResourceBinding*> m_SRVs;
		bool m_isLocked = false;
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

