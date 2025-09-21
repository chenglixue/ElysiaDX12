#include "BufferManager.h"
#include "stdafx.h"

namespace ElysiaRenderer
{
	using namespace ElysiaHelper;
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

	DX12BufferResource* BufferManager::GetSingleConstantBuffer(uint8_t spaceID) const noexcept
	{
		switch (spaceID)
		{
		case PER_PASS_SPACE:
			return m_pPassConstantBuffer.get();
			break;

		default:
		{
			AssertError("Invalid constant buffer space ID");
		}
		}

		return nullptr;

	}

	DX12BufferResource* BufferManager::GetMutilConstantBuffer(uint8_t spaceID, UINT frameID) const noexcept
	{
		switch (spaceID)
		{
		case PER_OBJECT_SPACE:
		{
			return m_objectConstantBuffers[frameID].get();
		}
		}
	}

	DX12TextureResource* BufferManager::GetCameraDepthBuffer() const noexcept
	{
		return m_pCameraDepthBuffer.get();
	}

	void BufferManager::AddConstantBuffer(uint8_t spaceID, std::unique_ptr<DX12BufferResource> pConstantBuffer)
	{
		switch (spaceID)
		{
			case PER_PASS_SPACE:
			{
				if (m_pPassConstantBuffer != nullptr)
				{
					m_pPassConstantBuffer.reset();
				}

				m_pPassConstantBuffer = std::move(pConstantBuffer);
			
				break;
			}

			case PER_OBJECT_SPACE:
			{
				for (int i = 0; i < m_objectConstantBuffers.size(); ++i)
				{
					if (m_objectConstantBuffers[i] != nullptr)
					{
						m_objectConstantBuffers[i].reset();
					}

					auto constantBuffer = *pConstantBuffer;

					m_objectConstantBuffers[i] = std::unique_ptr<DX12BufferResource>(&constantBuffer);
				}

				break;
			}
			default:
			{
				break;
			}
		}
	}

	void BufferManager::AddDepthBuffer(std::unique_ptr<DX12TextureResource> depthBuffer)
	{
		m_pCameraDepthBuffer = std::move(depthBuffer);
	}
}