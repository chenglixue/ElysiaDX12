#include "BufferManager.h"
#include "stdafx.h"

namespace ElysiaRenderer
{
	using namespace ElysiaHelper;

	BufferManager::BufferManager(DX12Device* pDevice)
		: m_pDevice(pDevice)
	{

	}
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
			break;
		}
		}

		return nullptr;
	}

	DX12TextureResource* BufferManager::GetCameraDepthBuffer() const noexcept
	{
		return m_pCameraDepthBuffer.get();
	}

	DX12BufferResource* BufferManager::GetVertexBuffer() const noexcept
	{
		return m_pVertexBuffer.get();
	}

	DX12BufferResource* BufferManager::GetIndexBuffer() const noexcept
	{
		return m_pIndexBuffer.get();
	}

	const D3D12_INDEX_BUFFER_VIEW& BufferManager::GetIndexBufferView() const noexcept
	{
		return m_indexBufferView;
	}

	const D3D12_VERTEX_BUFFER_VIEW& BufferManager::GetVertexBufferView() const noexcept
	{
		return m_vertexBufferView;
	}

	void BufferManager::AddConstantBuffer(uint8_t spaceID, BufferCreationDesc createDesc)
	{
		switch (spaceID)
		{
			case PER_PASS_SPACE:
			{
				/*if (m_pPassConstantBuffer != nullptr)
				{
					m_pPassConstantBuffer.reset();
				}*/

				m_pPassConstantBuffer = std::move(m_pDevice->CreateBuffer(createDesc));
			
				break;
			}

			case PER_OBJECT_SPACE:
			{
				for (int i = 0; i < m_objectConstantBuffers.size(); ++i)
				{
					/*if (m_objectConstantBuffers[i] != nullptr)
					{
						m_objectConstantBuffers[i].reset();
					}*/

					m_objectConstantBuffers[i] = std::move(m_pDevice->CreateBuffer(createDesc));
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

	void BufferManager::AddVertexBuffer(BufferCreationDesc desc)
	{
		m_pVertexBuffer = std::move(m_pDevice->CreateBuffer(desc));
	}

	void BufferManager::AddIndexBuffer(BufferCreationDesc desc)
	{
		m_pIndexBuffer = std::move(m_pDevice->CreateBuffer(desc));
	}

	void BufferManager::SetVertexBufferView(const D3D12_VERTEX_BUFFER_VIEW& view)
	{
		m_vertexBufferView = view;
	}

	void BufferManager::SetIndexBufferView(const D3D12_INDEX_BUFFER_VIEW& view)
	{
		m_indexBufferView = view;
	}

}