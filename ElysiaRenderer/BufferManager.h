#pragma once
#include "stdafx.h"
#include "IManager.h"
#include "DX12BufferResource.h"
#include "DX12TextureResource.h"

namespace ElysiaRenderer
{
	class BufferManager : public IManager
	{
	public:
		BufferManager() = default;
		BufferManager(const BufferManager& rhs) = delete;
		BufferManager& operator=(BufferManager& rhs) = delete;
		BufferManager(BufferManager&& rhs) = default;
		~BufferManager();

		virtual void Init() override;
		virtual void Destory() override;

		DX12BufferResource* GetSingleConstantBuffer(uint8_t spaceID) const noexcept;
		DX12BufferResource* GetMutilConstantBuffer(uint8_t spaceID, UINT frameID) const noexcept;
		DX12TextureResource* GetCameraDepthBuffer() const noexcept;

		void AddConstantBuffer(uint8_t spaceID, std::unique_ptr<DX12BufferResource> pConstantBuffer);
		void AddDepthBuffer(std::unique_ptr<DX12TextureResource> depthBuffer);


	private:
		std::array<std::unique_ptr<DX12BufferResource>, NUM_FRAMES_IN_FLIGHT> m_objectConstantBuffers{};
		std::unique_ptr<DX12BufferResource> m_pPassConstantBuffer = nullptr;

		std::unique_ptr<DX12TextureResource> m_pCameraDepthBuffer = nullptr;
	};
}