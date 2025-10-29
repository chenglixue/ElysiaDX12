#include "stdafx.h"
#include "BufferManager.h"

#include "DX12Device.h"
#include "DX12TextureBuffer.h"
#include "DX12BufferResource.h"
#include "RenderTexture.h"

namespace ElysiaRenderer
{
	std::unique_ptr<BufferManager> g_pBufferManager = nullptr;

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

	DX12BufferResource* BufferManager::GetMutilConstantBuffer(uint8_t spaceID, UINT frameID, UINT objectIndex) const noexcept
	{
		switch (spaceID)
		{
		case PER_OBJECT_SPACE:
		{
			return m_objectConstantBuffers[objectIndex * 2 + frameID].get();
			break;
		}
		}

		return nullptr;
	}

	RenderTexture* BufferManager::GetCameraDepthRT() const noexcept
	{
		return m_pCameraDepthBuffer;
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
				m_pPassConstantBuffer = std::move(GetDevice()->CreateBuffer(createDesc));
			
				break;
			}

			case PER_OBJECT_SPACE:
			{
				for (int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
				{
					m_objectConstantBuffers.emplace_back(std::move(GetDevice()->CreateBuffer(createDesc)));
				}

				break;
			}
			default:
			{
				break;
			}
		}
	}

	void BufferManager::AddDepthBuffer(RenderTexture* depthBuffer)
	{
		m_pCameraDepthBuffer = depthBuffer;
	}

	void BufferManager::AddVertexBuffer(BufferCreationDesc desc)
	{
		m_pVertexBuffer = std::move(GetDevice()->CreateBuffer(desc));
	}

	void BufferManager::AddIndexBuffer(BufferCreationDesc desc)
	{
		m_pIndexBuffer = std::move(GetDevice()->CreateBuffer(desc));
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