#pragma once

namespace ElysiaCore
{
	class DX12BufferResource;
	struct PipelineResourceBinding;
}

namespace ElysiaCore
{

	/// <summary>
	/// save all resource in root parameters
	/// </summary>
	class PipelineResourceSpace
	{
	public:
		PipelineResourceSpace();
		PipelineResourceSpace(const PipelineResourceSpace& rhs) = default;
		PipelineResourceSpace& operator=(const PipelineResourceSpace& rhs) = default;
		PipelineResourceSpace(PipelineResourceSpace&& rhs) = default;
		~PipelineResourceSpace();

		void Reset();
		
		void ExpectCBV(UINT registerIndex);
		void ExpectSRV(UINT registerIndex);
		void ExpectUAV(UINT registerIndex);
	    void ExpectPushConstant(UINT registerIndex);
		bool HasExpectedCBV() const noexcept;
		bool HasDynamicCBV() const noexcept;
	    bool IsPushConstantSpace() const noexcept;
		
		DX12BufferResource* GetStaticCBV() const;
		D3D12_GPU_VIRTUAL_ADDRESS GetDynamicCBV() const;
		std::vector<PipelineResourceBinding*>& GetSRVs() ;
		std::vector<PipelineResourceBinding*>& GetUAVs() ;
	    UINT GetPushConstantNumDWORDs() const noexcept { return m_pushConstantNumDWORDs; }
	    void SetPushConstantNumDWORDs(UINT count) { m_pushConstantNumDWORDs = count; }

		void SetStaticCBV(DX12BufferResource* CBVResource);
		void SetDynamicCBV(D3D12_GPU_VIRTUAL_ADDRESS GPUVA);
		void SetSRV(PipelineResourceBinding* SRVResource);
		void SetUAV(PipelineResourceBinding* UAVResource);

		void Lock();
		bool IsLocked() const;

	private:
		UINT GetIndexOfBindingIndex(const std::vector<PipelineResourceBinding*>& bindResources, UINT bindingIndex);
		enum class ResourceType { CBV, SRV, UAV, PushConstant };
		std::map<UINT, ResourceType> m_expectedBindings;

		DX12BufferResource* m_pStaticCBV = nullptr;
		D3D12_GPU_VIRTUAL_ADDRESS m_dynamicCBVAddress = 0;
		std::vector<PipelineResourceBinding*> m_SRVs;
		std::vector<PipelineResourceBinding*> m_UAVs;
		bool m_isLocked = false;
		bool m_hasDynamicCBV = false;
	    UINT m_pushConstantNumDWORDs = 0;
	};
}

