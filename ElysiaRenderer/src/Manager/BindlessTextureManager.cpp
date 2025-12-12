#include "stdafx.h"
#include "BindlessTextureManager.h"

#include "DX12/DX12Device.h"
#include "DX12/DX12RenderPassDescriptorHeap.h"
#include "DX12/DX12StagingDescriptorHeap.h"
#include "DX12/DX12TextureBuffer.h"
#include "DX12/DX12UploadContext.h"

namespace ElysiaRenderer
{
	void BindlessTextureManager::Init(DX12Device* pDevice)
	{
		assert(pDevice);
		m_pDevice = pDevice;
	}

	void BindlessTextureManager::Destory()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_textures.clear();
	}

	BindlessTextureManager::TextureHandle BindlessTextureManager::CreateTextureFromFile(const std::wstring& filePath, bool isSRGB)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		if (m_textures.size() >= m_maxTextureNum)
		{
			return TextureHandle::Invalid();
		}

		auto pNewTex = std::move(LoadTextureFromFile_L(filePath, isSRGB));
		if (!pNewTex)
		{
			return TextureHandle::Invalid();
		}

		UINT index = static_cast<UINT>(m_textures.size());
		UINT resourceHeapIndex = pNewTex->GetResourceHeapIndex();
		
		m_textures.emplace_back(std::move(pNewTex));
		
		return {index, resourceHeapIndex};
	}

	BindlessTextureManager::TextureHandle BindlessTextureManager::CreateTexture(const D3D12_RESOURCE_DESC& resourceDesc, TexTypeFlags flag, std::wstring name)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		if (m_textures.size() >= m_maxTextureNum)
		{
			return TextureHandle::Invalid();
		}
		auto pNewTex = std::move(CreateTexture_L(resourceDesc, flag, name));
		if (!pNewTex)
		{
			return TextureHandle::Invalid();
		}
		
		UINT index = static_cast<UINT>(m_textures.size());
		UINT resourceHeapIndex = pNewTex->GetResourceHeapIndex();

		m_textures.emplace_back(std::move(pNewTex));

		return {index, resourceHeapIndex};
	}

	DX12TextureResource* BindlessTextureManager::GetTexture(TextureHandle handle) const noexcept
	{
		if (!handle.IsValid() || handle.textureIndex >= m_textures.size())
		{
			return nullptr;
		}

		return m_textures[handle.textureIndex].get();
	}

	UINT BindlessTextureManager::GetMaxCapicity() const noexcept
	{
		return m_maxTextureNum;
	}

	UINT BindlessTextureManager::GetUsedCount() const noexcept
	{
		return static_cast<UINT>(m_textures.size());
	}

	std::unique_ptr<DX12TextureResource> BindlessTextureManager::LoadTextureFromFile_L(const std::wstring& filePath, bool isSRGB)
	{
		auto texturePath = filePath;
		IsFileLocked(texturePath);

		const std::wstring extension = GetFileExtension(texturePath.c_str());
		/// Load DDS
		std::unique_ptr<DirectX::ScratchImage> imageData = std::make_unique<DirectX::ScratchImage>();
		if(extension == L"DDS" || extension == L"dds")
		{
			auto s2ws = [](const std::string& s)
			{
				//yoink https://stackoverflow.com/questions/27220/how-to-convert-stdstring-to-lpcwstr-in-c-unicode
				int32_t len = 0;
				int32_t slength = (int32_t)s.length() + 1;
				len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
				wchar_t* buf = new wchar_t[len];
				MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
				std::wstring r(buf);
				delete[] buf;
				return r;
			};

			WCHAR assetsPath[512];
			ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));

			if (!std::filesystem::exists(WstringToString(assetsPath + texturePath)))
			{
				return nullptr;
			}

			imageData = std::make_unique<DirectX::ScratchImage>();
			auto loadResult = DirectX::LoadFromDDSFile((assetsPath + texturePath).c_str(), DirectX::DDS_FLAGS_NONE, nullptr, *imageData);
			if (loadResult != S_OK)
			{
				std::cout << WstringToString(texturePath) + " not found" << std::endl;
				return nullptr;
			}
		}
		else
		{
			if (!std::filesystem::exists(WstringToString(texturePath)))
			{
				return nullptr;
			}
			DirectX::ScratchImage tempImage;
			auto loadResult = DirectX::LoadFromWICFile(texturePath.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, tempImage);
			if (loadResult != S_OK)
			{
				if (!LoadWithSTB(texturePath, tempImage))
				{
					// PrintPathInfo(texturePath);
					// TestFileAccess(texturePath);
					//std::cout << WstringToString(textureCreationDesc.texturePath) + " not found" << std::endl;
					return nullptr;
				}
				
			}
			ThrowIfFailed(DirectX::GenerateMipMaps(*tempImage.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, 0, *imageData, false));
		}
		///

		/// grad tex data
		///
		const auto& texMetaData = imageData->GetMetadata();
		auto texFormat = isSRGB ? DirectX::MakeSRGB(texMetaData.format) : texMetaData.format;
		bool is3DTex = texMetaData.dimension == DirectX::TEX_DIMENSION_TEXTURE3D;
		///
		
		/// Create tex desc && tex resource
		D3D12_RESOURCE_DESC texDesc{};
		texDesc.Width = texMetaData.width;
		texDesc.Height = static_cast<UINT>(texMetaData.height);
		texDesc.Dimension = is3DTex ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Format = texFormat;
		texDesc.MipLevels = static_cast<UINT16>(texMetaData.mipLevels);
		texDesc.Alignment = 0;
		texDesc.DepthOrArraySize = static_cast<UINT16>(is3DTex ? texMetaData.depth : texMetaData.arraySize);
		texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

		auto newTex = std::move(this->CreateTexture_L(texDesc, TexTypeFlags::SRV));

		auto textureUpload = new DX12TextureUpload();
		textureUpload->m_textureBuffer = newTex.get();
		textureUpload->m_numSubResources = static_cast<UINT>(texMetaData.mipLevels * texMetaData.arraySize);

		UINT numRows[MAX_TEXTURE_SUBRESOURCE_COUNT];
		uint64_t rowSizesInBytes[MAX_TEXTURE_SUBRESOURCE_COUNT];

		auto resourceDesc = textureUpload->m_textureBuffer->GetResourceDesc();
		m_pDevice->GetDevice()->GetCopyableFootprints(&resourceDesc, 0, textureUpload->m_numSubResources, 0,
			textureUpload->m_subResourceLayouts.data(), numRows, rowSizesInBytes, &textureUpload->m_textureDataSize);
		
		textureUpload->m_pTextureData = std::make_unique<uint8_t[]>(textureUpload->m_textureDataSize);

		for (size_t arrayIndex = 0; arrayIndex < texMetaData.arraySize; ++arrayIndex)
		{
			for (size_t mipIndex = 0; mipIndex < texMetaData.mipLevels; ++mipIndex)
			{
				const uint64_t subResourceIndex = mipIndex + (arrayIndex * texMetaData.mipLevels);

				const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& subResourcelayout = textureUpload->m_subResourceLayouts[subResourceIndex];
				const uint64_t subResourceHeight = numRows[subResourceIndex];
				const uint64_t subResourcePitch = ElysiaHelper::AlignU32(subResourcelayout.Footprint.RowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
				const uint64_t subResourceDepth = subResourcelayout.Footprint.Depth;
				uint8_t* destSubResourceMemory = textureUpload->m_pTextureData.get() + subResourcelayout.Offset;

				for (uint64_t sliceIndex = 0; sliceIndex < subResourceDepth; sliceIndex++)
				{
					const auto subImage = imageData->GetImage(mipIndex, arrayIndex, sliceIndex);
					const uint8_t* sourceSubResourceMemory = subImage->pixels;
					for (uint64_t height = 0; height < subResourceHeight; ++height)
					{
						memcpy(destSubResourceMemory, sourceSubResourceMemory, (std::min)(subResourcePitch, subImage->rowPitch));
						destSubResourceMemory += subResourcePitch;
						sourceSubResourceMemory += subImage->rowPitch;
					}
				}
			}
		}

		m_pDevice->GetUploadContext()->AddTextureToUploads(std::move(textureUpload));

		return newTex;
	}

	std::unique_ptr<DX12TextureResource> BindlessTextureManager::CreateTexture_L(const D3D12_RESOURCE_DESC& desc, TexTypeFlags typeFlag, std::wstring name)
	{
		auto resourceDesc = desc;

		bool hasRTV = (typeFlag & TexTypeFlags::RTV) == TexTypeFlags::RTV;
		bool hasSRV = (typeFlag & TexTypeFlags::SRV) == TexTypeFlags::SRV;
		bool hasDSV = (typeFlag & TexTypeFlags::DSV) == TexTypeFlags::DSV;
		bool hasUAV = (typeFlag & TexTypeFlags::UAV) == TexTypeFlags::UAV;

		DXGI_FORMAT resourceFormat = resourceDesc.Format;
		DXGI_FORMAT shaderResourceViewFormat = resourceDesc.Format;
		//D3D12_RESOURCE_STATES usageState = D3D12_RESOURCE_STATE_COPY_DEST;
		D3D12_RESOURCE_STATES usageState = D3D12_RESOURCE_STATE_COMMON;

		if (hasRTV)
		{
			resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			usageState = D3D12_RESOURCE_STATE_RENDER_TARGET;
		}

		if (hasDSV)
		{
			switch(desc.Format)
			{
				case DXGI_FORMAT_D16_UNORM:
				{
					resourceFormat = DXGI_FORMAT_R16_TYPELESS;
					shaderResourceViewFormat = DXGI_FORMAT_R16_UNORM;
					break;
				}
				case DXGI_FORMAT_D24_UNORM_S8_UINT:
				{
					resourceFormat = DXGI_FORMAT_R24G8_TYPELESS;
					shaderResourceViewFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
					break;
				}
				case DXGI_FORMAT_D32_FLOAT:
				{
					resourceFormat = DXGI_FORMAT_R32_TYPELESS;
					shaderResourceViewFormat = DXGI_FORMAT_R32_FLOAT;
					break;
				}
				case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
				{
					resourceFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
					shaderResourceViewFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
					break;
				}
				default:
				{
					ElysiaHelper::AssertError("Bad depth stencil format.");
					break;
				}
			}

			resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			//usageState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
			usageState = D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		}

		if (hasUAV)
		{
			resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			usageState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		resourceDesc.Format = resourceFormat;

		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = desc.Format;
		if (hasDSV)
		{
			clearValue.DepthStencil.Depth = 1.0f;
			clearValue.DepthStencil.Stencil = 0;
		}
		if (hasRTV)
		{
			float clearColor[4] = { 0.f, 0.f, 0.f, 0.f };
			memcpy(clearValue.Color, clearColor, sizeof(clearValue.Color));
		}

		/// Create default heap for tex
		D3D12MA::ALLOCATION_DESC allocationDesc{};
		allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
		CComPtr<ID3D12Resource> texResource = nullptr;
		CComPtr<D3D12MA::Allocation> allocation = nullptr;
		ElysiaHelper::ThrowIfFailed(m_pDevice->GetAllocator()->CreateResource(&allocationDesc, &resourceDesc, usageState, (!hasRTV && !hasDSV) ? nullptr : &clearValue,
			&allocation, IID_PPV_ARGS(&texResource)));
		texResource->SetName(name.c_str());
		/// 

		auto newTex = std::make_unique<DX12TextureResource>(texResource, usageState, allocation);

		if (hasSRV)
		{
			auto SRVHandle = m_pDevice->GetSRVStageHeap()->NewDescriptorHeapHandle();

			if (hasDSV)
			{
				D3D12_SHADER_RESOURCE_VIEW_DESC SRV{};
				SRV.Format = shaderResourceViewFormat;
				SRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				SRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				SRV.Texture2D.MostDetailedMip = 0;
				SRV.Texture2D.MipLevels = 1;
				SRV.Texture2D.ResourceMinLODClamp = 0;
				SRV.Texture2D.PlaneSlice = 0;

				m_pDevice->GetDevice()->CreateShaderResourceView(newTex->GetResource(), &SRV, SRVHandle.GetCPUHandle());
			}
			else
			{
				D3D12_SHADER_RESOURCE_VIEW_DESC* srvDescPointer = nullptr;
				D3D12_SHADER_RESOURCE_VIEW_DESC SRV = {};

				bool isCubeMap = resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && resourceDesc.DepthOrArraySize == 6;
				if (isCubeMap)
				{
					SRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
					SRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
					SRV.TextureCube.MostDetailedMip = 0;
					SRV.TextureCube.MipLevels = (UINT)resourceDesc.MipLevels;
					SRV.TextureCube.ResourceMinLODClamp = 0;
					srvDescPointer = &SRV;
				}

				m_pDevice->GetDevice()->CreateShaderResourceView(newTex->GetResource(), srvDescPointer, SRVHandle.GetCPUHandle());
			}
			
			///
			newTex->SetSRVDescriptor(SRVHandle);
			newTex->SetResourceHeapIndex(m_pDevice->m_freeReservedDescriptorIndices.back());
			m_pDevice->m_freeReservedDescriptorIndices.pop_back();

			m_pDevice->CopyDescriptorFromStageToRenderPass(newTex->GetSRVDescriptor(), newTex->GetResourceHeapIndex());
		}

		if (hasRTV)
		{
			auto RTVHandle = m_pDevice->GetRTVStageHeap()->NewDescriptorHeapHandle();

			newTex->SetRTVDescriptor(RTVHandle);
			m_pDevice->GetDevice()->CreateRenderTargetView(newTex->GetResource(), nullptr, newTex->GetRTVDescriptor().GetCPUHandle());

		}

		if (hasDSV)
		{
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
			dsvDesc.Format = desc.Format;
			dsvDesc.Texture2D.MipSlice = 0;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

			auto newDSVHandle = m_pDevice->GetDSVStageHeap()->NewDescriptorHeapHandle();
			newTex->SetDSVDescriptor(newDSVHandle);
			m_pDevice->GetDevice()->CreateDepthStencilView(newTex->GetResource(), &dsvDesc, newTex->GetDSVDescriptor().GetCPUHandle());
		}

		if (hasUAV)
		{
			auto newUAVHandle = m_pDevice->GetSRVStageHeap()->NewDescriptorHeapHandle();
			newTex->SetUAVDescriptor(newUAVHandle);
			m_pDevice->GetDevice()->CreateUnorderedAccessView(newTex->GetResource(), nullptr, nullptr, newTex->GetUAVDescriptor().GetCPUHandle());
		}

		newTex->SetIsReady(hasRTV || hasDSV);

		return newTex;
	}
}
