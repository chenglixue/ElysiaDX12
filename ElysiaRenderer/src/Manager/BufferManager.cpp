#include "stdafx.h"
#include "BufferManager.h"

#include "lib/DX12/DX12Device.h"
#include "lib/DX12/DX12TextureBuffer.h"
#include "lib/DX12/DX12BufferResource.h"
#include "lib/Utility/RenderTexture.h"
#include "Parameter/UserData.h"

namespace ElysiaRenderer
{
	std::unique_ptr<BufferManager> g_pBufferManager = nullptr;

	BufferManager::~BufferManager()
	{
		Destory();
	}

	void BufferManager::Init()
	{
		if (!UserData::GetInstance().IsUseHDR)
		{
			m_pCameraColorRT = CreateRWRenderTexture(static_cast<UINT64>(GetDevice()->GetScreenSize().x),
				static_cast<UINT64>(GetDevice()->GetScreenSize().y),
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
				true,
				L"Camera Color RT");
		}
		else
		{
			switch (UserData::GetInstance().HDRLevel)
			{
				case HDRQuality::Low: 
				{
					m_pCameraColorRT = CreateRWRenderTexture(static_cast<UINT64>(GetDevice()->GetScreenSize().x),
						static_cast<UINT64>(GetDevice()->GetScreenSize().y),
						DXGI_FORMAT_R11G11B10_FLOAT,
						true,
						L"Camera Color RT"); 
					break;  
				} 
				case HDRQuality::High:
				{ 
					m_pCameraColorRT = CreateRWRenderTexture(static_cast<UINT64>(GetDevice()->GetScreenSize().x),
						static_cast<UINT64>(GetDevice()->GetScreenSize().y),
						DXGI_FORMAT_R16G16B16A16_FLOAT,
						true,
						L"Camera Color RT");
					break;
				}
				default:
				{ 
					ThrowRuntimeError("Invalid choose");
					break;
				}
			}
		}
		m_pCameraDepthRT = CreateRenderTexture(
			static_cast<UINT64>(GetDevice()->GetScreenSize().x),
			static_cast<UINT64>(GetDevice()->GetScreenSize().y),
			DXGI_FORMAT_D24_UNORM_S8_UINT,
			true,
			L"Camera Depth RT");
	}

	void BufferManager::Destory()
	{

	}

	void BufferManager::Update() 
	{
		
	}

	DX12BufferResource* BufferManager::GetSingleConstantBuffer(uint8_t spaceID) const noexcept
	{
		switch (spaceID)
		{
		case PER_PASS_SPACE:
		{
			return m_pPassConstantBuffer.get();
			break;
		}

		case PER_FRAME_SPACE:
		{
			return m_pFrameConstantBuffer.get();
			break;
		}
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
		return m_pCameraDepthRT.get();
	}

	RenderTexture* BufferManager::GetCameraColorRT() const noexcept
	{
		return m_pCameraColorRT.get();
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

			case PER_FRAME_SPACE:
			{
				m_pFrameConstantBuffer = std::move(GetDevice()->CreateBuffer(createDesc));
				break;
			}
			default:
			{
				break;
			}
		}
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