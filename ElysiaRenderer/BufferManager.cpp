#include "BufferManager.h"

namespace ElysiaRenderer
{
	BufferManager::~BufferManager()
	{
		Destory();
	}

	void BufferManager::Init()
	{

	}

	void BufferManager::Destory()
	{

	}

	DX12BufferResource* BufferManager::GetConstantBuffer(uint8_t spaceID) const noexcept
	{
		return m_constantBuffers[spaceID].get();
	}

	void BufferManager::AddConstantBuffer(uint8_t spaceID, std::unique_ptr<DX12BufferResource> pConstantBuffer)
	{
		if (m_constantBuffers[spaceID])
		{
			m_constantBuffers[spaceID].reset();
		}

		m_constantBuffers[spaceID] = std::move(pConstantBuffer);
	}
}