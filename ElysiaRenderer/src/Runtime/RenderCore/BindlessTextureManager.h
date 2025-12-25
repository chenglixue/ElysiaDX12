#pragma once
#include "Programs/Helper.h"
#include "Programs/IManager.h"
#include "Runtime/Core/DX12DescriptorHeapHandle.h"
#include "Runtime/Core/TextureUtility.h"

namespace ElysiaCore
{
	class DX12RenderPassDescriptorHeap;
	class DX12StagingDescriptorHeap;
}

namespace ElysiaRenderer
{
	using namespace ElysiaCore;
	
	class BindlessTextureManager : IManager
	{
	public:
		struct TextureHandle
		{
			UINT textureIndex;
			UINT resourceHeapIndex = UINT_MAX;
			bool IsValid() const { return textureIndex != ~0u && resourceHeapIndex != UINT_MAX; }
			static TextureHandle Invalid() { return { ~0u }; }
		};
		
	public:
		BindlessTextureManager();
		~BindlessTextureManager();
		
		virtual void Init(DX12Device* pDevice) override;
		virtual void Destory() override;
		
		TextureHandle CreateTextureFromFile(const std::wstring& filePath, bool isSRGB = false);
		TextureHandle CreateTexture(const D3D12_RESOURCE_DESC& resourceDesc, TexTypeFlags flag, std::wstring name = L"");
		
		DX12TextureResource* GetTexture(TextureHandle handle) const noexcept;
		UINT GetMaxCapicity() const noexcept;
		UINT GetUsedCount() const noexcept;
		
		void Clear();
		
	private:
		DX12Device* m_pDevice = nullptr;
		
		std::mutex                  m_mutex;
		
		std::vector<std::unique_ptr<DX12TextureResource>> m_textures;
		DX12DescriptorHeapHandle m_startHeapHandle;
		
		static inline constexpr UINT m_maxTextureNum = 4096;

		std::unique_ptr<DX12TextureResource>	LoadTextureFromFile_L(const std::wstring& filePath, bool isSRGB);
		std::unique_ptr<DX12TextureResource>	CreateTexture_L(const D3D12_RESOURCE_DESC& resourceDesc, TexTypeFlags flag, std::wstring name = L"");
	};
}

