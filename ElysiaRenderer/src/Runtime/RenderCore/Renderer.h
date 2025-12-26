#pragma once

#include "Programs/Helper.h"
#include "Pass/BasePass.h"
#include "Runtime/Developer/GPUTimestamps.h"
#include "Runtime/Engine/FrameContext.h"

namespace ElysiaEditor
{
	class IMGUI;
}

namespace ElysiaCore
{
	class SwapChain;
	struct PipelineStateObject;
	class DX12TextureResource;
}

namespace ElysiaEditor
{
	class DX12UI;
}

namespace ElysiaRenderer
{
	class MeshRenderer;
	class MeshManager;
	class TextureManager;
	class CameraManager;
}

namespace ElysiaRenderer 
{
	using namespace ElysiaHelper;
	using namespace ElysiaCore;
	using namespace ElysiaEditor;
	
	class Renderer
	{
	public:
		Renderer();
		~Renderer();

		void OnCreateWindowSizeDependentResources(SwapChain *pSwapChain, uint32_t Width, uint32_t Height);
		void OnDestroyWindowSizeDependentResources();
		void OnUpdateDisplayDependentResources(SwapChain* pSwapChain);

		void OnCreate(DX12Device* pDevice, SwapChain* pSwapChain);
		void OnRender(ElysiaEngine::FrameContext frameContext); 
		void OnDestory();

		const std::vector<TimeStamp> &GetTimingValues() { return m_TimeStamps; }

	protected:
		DX12Device*						m_pDevice = nullptr;
		std::unique_ptr<IMGUI>			m_pImGui = nullptr;

		uint32_t                        m_Width;
		uint32_t                        m_Height;
		D3D12_VIEWPORT                  m_viewport;
		D3D12_RECT                      m_rectScissor;
		
		GPUTimestamps					m_GPUTimer;
		std::vector<TimeStamp>          m_TimeStamps;

		std::unique_ptr<ElysiaCore::DX12GraphicsContext> m_graphicsContext = nullptr;
		std::vector<std::unique_ptr<D3D12_SAMPLER_DESC>> m_samplers{};
		std::vector<std::unique_ptr<BasePass>> m_passes{};
		eastl::vector<std::unique_ptr<MeshRenderer>> m_meshRenderers;
		RenderTexture* m_pCameraColorRT = nullptr;
		RenderTexture* m_pCameraDepthRT = nullptr;
	};     
}   
          