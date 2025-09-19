#pragma once
#include "stdafx.h"
#include "IManager.h"
#include "DX12BufferResource.h"

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

		DX12BufferResource* GetConstantBuffer(uint8_t spaceID) const noexcept;

		void AddConstantBuffer(uint8_t spaceID, std::unique_ptr<DX12BufferResource> pConstantBuffer);

	private:
		std::array<std::unique_ptr<DX12BufferResource>, NUM_RESOURCE_SPACES> m_constantBuffers{};
	};
}