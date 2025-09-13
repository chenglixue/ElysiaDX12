#pragma once
#include "stdafx.h"
#include "DX12TextureResource.h"
#include "DX12ConstantBuffer.h"

namespace ElysiaRenderer
{
	struct PipelineResourceBinding
	{
		uint32_t m_bindingIndex = 0;
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

		const DX12ConstantBuffer* GetCBV() const
		{
			return m_CBV.get();
		}
		const std::vector<std::shared_ptr<DX12TextureResource>>& GetSRVs() const
		{
			return m_SRVs;
		}

		void SetCBV(std::shared_ptr<DX12ConstantBuffer> CBVResource);
		void SetSRV(std::shared_ptr<PipelineResourceBinding> SRVResource);

		void Lock()
		{
			m_isLocked = true;
		}
		bool IsLocked() const
		{
			return m_isLocked;
		}
		
		/*std::unordered_map<UINT, std::vector<std::shared_ptr<DX12GPUResource>>> m_SRVResources{};
		std::unordered_map<UINT, std::vector<std::shared_ptr<DX12ConstantBuffer>>> m_CBVResource{};
		std::unordered_map<UINT, UINT> CBVSizes{};
		std::unordered_map<UINT, size_t> CBVIndexs{};*/

	private:
		UINT GetIndexOfBindingIndex(const std::vector<std::shared_ptr<PipelineResourceBinding>>& bindResources, UINT bindingIndex);

		std::shared_ptr<DX12ConstantBuffer> m_CBV;
		std::vector<std::shared_ptr<PipelineResourceBinding>> m_SRVs;
		bool m_isLocked = false;
	};

	struct PipelineResourceLayout
	{
		std::array<std::shared_ptr<PipelineResourceSpace>, NUM_RESOURCE_SPACES> mSpaces{ nullptr };
	};
}

