#pragma once
#include "PipelineResourceUtility.h"
#include "BufferUtility.h"

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

		const BufferCreationDesc& GetCBVDesc() const noexcept;
		DX12BufferResource* GetCBV();
		std::vector<PipelineResourceBinding*>& GetSRVs();
		std::vector<PipelineResourceBinding*>& GetUAVs();

		void SetCBVDesc(const BufferCreationDesc&);
		void SetCBV(DX12BufferResource* CBVResource);
		void SetSRV(PipelineResourceBinding* SRVResource);
		void SetUAV(PipelineResourceBinding* UAVResource);

		void Lock();
		bool IsLocked() const;

	private:
		UINT GetIndexOfBindingIndex(const std::vector<PipelineResourceBinding*>& bindResources, UINT bindingIndex);

		BufferCreationDesc m_CBVDesc;
		DX12BufferResource* m_CBV;
		std::vector<PipelineResourceBinding*> m_SRVs;
		std::vector<PipelineResourceBinding*> m_UAVs;
		bool m_isLocked = false;
	};
}

