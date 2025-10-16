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
#include "ShadowManager.h"
#include "BufferManager.h"
#include "MeshManager.h"
#include "TextureManager.h"
#include "RenderResource.h"
#include "ModelImporter.h"
#include "UserData.h"
#include "Common.h"  

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
		Renderer* m_render = nullptr;
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
		std::unique_ptr<DX12Device> m_device = nullptr;
		std::unique_ptr<DX12GraphicsContext> m_graphicsContext = nullptr;
		std::vector<std::unique_ptr<DX12TextureResource>> m_texs{};
		std::vector<std::unique_ptr<D3D12_SAMPLER_DESC>> m_samplers{};
		std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>> m_vertexShaders;
		std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>> m_pixelShaders;
		std::vector<std::unique_ptr<DX12Shader>> m_computeShaders;
		std::unordered_map<UINT, std::shared_ptr<PipelineStateObject>> m_graphicsPipelineStates;

		std::unique_ptr<ModelImporter> m_pModelImporter = nullptr;

		std::unique_ptr<CameraManager>	m_pCameraManager = nullptr;  
		std::unique_ptr<LightManager>	m_pLightManager = nullptr;
		std::unique_ptr<ShadowManager>	m_pShadowManager = nullptr;
		std::unique_ptr<BufferManager>	m_pBufferManager = nullptr;
		std::unique_ptr<RenderResource> m_pRenderSource = nullptr;
		std::unique_ptr<MeshManager>	m_pMeshManager = nullptr;
		std::unique_ptr<TextureManager>	m_pTextureManager = nullptr;
		
		std::unique_ptr<PipelineResourceSpace> m_perObjectBindResourceSpace = nullptr;
		std::unique_ptr<PipelineResourceSpace> m_perMainPassBindResourceSpace = nullptr;

		void UpdateCBV();
		void UpdatePassCBV();
		void UpdateObjectCBV(); 
		    
		void InitTexTriangle();
		void LoadShaders();
		void LoadConstantBuffers();
		void CreateCreamDepthRT();
		void LoadTextures(); 
		void CreatePOS();

		void AddShader(ShaderQueue shaderQueue, const std::wstring& shaderName, const std::wstring& entryPoint, ShaderType shaderType);
	
		void RenderTexTriangle();
		
		void AddUIItems();
		void DrawShadow();
		void DrawOpaque(); 
		void DrawSkybox();
		void DrawUI();
	}; 
}
     