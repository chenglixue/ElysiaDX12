#pragma once
#include "PipelineResourceUtility.h"

namespace ElysiaRenderer
{
	class DX12BufferResource;

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

		DX12BufferResource* GetCBV();
		std::vector<PipelineResourceBinding*>& GetSRVs();

		void SetCBV(DX12BufferResource* CBVResource);
		void SetSRV(PipelineResourceBinding* SRVResource);

		void Lock();
		bool IsLocked() const;

	private:
		UINT GetIndexOfBindingIndex(const std::vector<PipelineResourceBinding*>& bindResources, UINT bindingIndex);

		DX12BufferResource* m_CBV;
		std::vector<PipelineResourceBinding*> m_SRVs;
		bool m_isLocked = false;
	};
}

