#pragma once
#include "lib/Utility/Helper.h"
#include "IManager.h"

namespace ElysiaRenderer
{
	class DX12TextureResource;
	class BindlessTextureManager : IManager
	{
	public:
		struct Handle
		{
			UINT index;
			bool IsValid() const { return index != ~0u; }
			static Handle Invalid() { return { ~0u }; }
		};
		
	public:
		static BindlessTextureManager& GetInstance()
		{
			std::call_once(m_initInstanceFlag, []()
			{
				m_instance.reset(new BindlessTextureManager());
			});

			return *m_instance;
		}
		
		virtual void Init(DX12Device* pDevice) override;
		virtual void Destory() override;
		
		Handle CreateTextureFromFile(const std::wstring& filePath, bool isSRGB = false);
		Handle CreateCubeMapFromFile(const std::wstring& filePath, bool isSRGB = false);
		
		D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(UINT index) const ;
		D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(UINT index) const ;
		D3D12_GPU_DESCRIPTOR_HANDLE GetGPUBaseHandle() const noexcept;
		
		DX12TextureResource* GetTexture(Handle handle) const noexcept;
		UINT GetMaxCapicity() const noexcept;
		UINT GetUsedCount() const;
		
		void Clear();
		
	private:
		DX12Device* m_pDevice = nullptr;
		static std::unique_ptr<BindlessTextureManager> m_instance;
		static std::once_flag m_initInstanceFlag;
	};
}

