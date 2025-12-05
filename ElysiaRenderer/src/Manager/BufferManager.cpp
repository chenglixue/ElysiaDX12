#include "stdafx.h"
#include "BufferManager.h"

#include "RenderTargetManager.h"
#include "lib/DX12/DX12Device.h"
#include "lib/DX12/DX12TextureBuffer.h"
#include "lib/DX12/DX12BufferResource.h"
#include "Parameter/UserData.h"

namespace ElysiaRenderer
{
	std::unique_ptr<BufferManager> BufferManager::m_instance;
	std::once_flag BufferManager::m_initInstanceFlag;
	
	BufferManager::~BufferManager()
	{
		Destory();
	}

	void BufferManager::Init(DX12Device* pDevice)
	{
		assert(pDevice);
		m_pDevice = pDevice;
		
		if (!UserData::GetInstance().IsUseHDR)
		{
			m_pCameraColorRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(static_cast<UINT64>(m_pDevice->GetScreenSize().x),
				static_cast<UINT64>(m_pDevice->GetScreenSize().y),
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
					m_pCameraColorRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(static_cast<UINT64>(m_pDevice->GetScreenSize().x),
						static_cast<UINT64>(m_pDevice->GetScreenSize().y),
						DXGI_FORMAT_R11G11B10_FLOAT,
						true,
						L"Camera Color RT"); 
					break;  
				} 
				case HDRQuality::High:
				{ 
					m_pCameraColorRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(static_cast<UINT64>(m_pDevice->GetScreenSize().x),
						static_cast<UINT64>(m_pDevice->GetScreenSize().y),
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
		m_pCameraDepthRT = RenderTargetManager::GetInstance().CreateRenderTexture(
			static_cast<UINT64>(m_pDevice->GetScreenSize().x),
			static_cast<UINT64>(m_pDevice->GetScreenSize().y),
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