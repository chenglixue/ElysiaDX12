#pragma once

#include "stdafx.h"
#include "DX12Device.h"
#include "DX12MeshRender.h"
#include "DX12Light.h"
#include "DX12UI.h"
#include <dxgidebug.h>
#include "DX12Shadow.h"  
#include "CBVParameter.h"
#include "LoadTexData.h"
#include "CameraManager.h"
#include "LightManager.h"
#include "BufferManager.h"
#include "MeshManager.h"
#include "SobolSequenceGenerator.h"
#include "ShadowPass.h"
#include "GBufferPass.h"

namespace ElysiaRenderer 
{
	using namespace ElysiaHelper;
	using namespace ElysiaModel;
	using namespace DirectX::SimpleMath;
	
	class Renderer
	{
	public:
		Renderer(HWND windowHandle, ElysiaHelper::UINT2 screenSize, std::shared_ptr<DX12UI> pUI);
		~Renderer();

		void Init();
		void Update();
		void Render(); 
		void Destory();
		void Resize();

		virtual void OnMouseDown(WPARAM btnState, int x, int y);
		virtual void OnMouseUp(WPARAM btnState, int x, int y);
		virtual void OnMouseMove(WPARAM btnState, int x, int y);
		virtual void OnKeyboardInput();

		bool IsStopped() const
		{
			return m_isStopped;
		}
		void SetIsStopped(bool isStopped)
		{
			m_isStopped = isStopped;
		}
		bool IsMin() const
		{
			return m_isMin;
		}
		void SetIsMin(bool isMin)
		{
			m_isMin = isMin;
		}
		bool IsMax() const
		{
			return m_isMax;
		}
		void SetIsMax(bool isMax)
		{
			m_isMax = isMax;
		}
		bool IsResizing() const
		{
			return m_isResizing;
		}
		void SetIsResizing(bool isResizing)
		{
			m_isResizing = isResizing;
		}

	protected:
		HWND m_windowHandle; 

		bool m_isStopped = false;
		bool m_isMin = false;
		bool m_isMax = false;
		bool m_isResizing = false;

		XMINT2 m_lastMousePos{};

		/// <summary>
		/// pipeline
		/// </summary>
		float m_aspectRatio;
		std::shared_ptr<DX12UI> m_pUI = nullptr;
		std::unique_ptr<DX12GraphicsContext> m_graphicsContext = nullptr;
		std::vector<std::unique_ptr<DX12TextureResource>> m_texs{};
		std::vector<std::unique_ptr<D3D12_SAMPLER_DESC>> m_samplers{};
		std::unordered_map<UINT, std::unique_ptr<PipelineStateObject>> m_graphicsPipelineStates{};
		std::vector<std::unique_ptr<BasePass>> m_passes{};

		std::unique_ptr<CameraManager>	m_pCameraManager = nullptr;  
		std::unique_ptr<MeshManager>	m_pMeshManager = nullptr;
		std::unique_ptr<TextureManager>	m_pTextureManager = nullptr;

		void UpdateCBV();
		void UpdatePassCBV();
		void UpdateObjectCBV(); 
		
		void Setup();
		void LoadShaders();
		void CreateConstantBuffers();
		void CreateCreamDepthRT();
		void LoadTextures(); 
		void CreatePOS();
		
		void Execute();
		
		void AddUIItems();
		void DrawOpaque(); 
		void DrawSkybox();
		void DrawUI();
	};  
}
     