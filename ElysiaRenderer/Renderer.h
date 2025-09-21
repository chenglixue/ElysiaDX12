#pragma once

#include "stdafx.h"
#include "DX12Device.h"
#include "DX12MeshRender.h"
#include "DX12Light.h"
#include "DX12UI.h"
#include <dxgidebug.h>
#include "DX12Shadow.h"
#include "CBVPassParameter.h"
#include "LoadTexData.h"
#include "CameraManager.h"
#include "LightManager.h"
#include "ShadowManager.h"
#include "BufferManager.h"
#include "RenderResource.h"


namespace ElysiaRenderer 
{
	using namespace ElysiaHelper;
	using namespace DirectX::SimpleMath;

	const std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputElementDescs =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
	/// <summary>
	/// user data
	/// </summary> 
	const std::vector<LPCWSTR> m_modelPaths
	{ 
		L"Mesh\\LOW_WEPON.fbx",
		//L"Mesh\\plane.fbx",
		L"Mesh\\Sphere.fbx",
	};
	const std::vector<TexLoadSetting> m_globalTexLoadSettings
	{
		{L"Tex\\GGX_E_LUT.dds"},
		{L"Tex\\GGX_Eavg_LUT.dds"},
		{L"Tex\\cubemap0.dds", true},
	};
	const std::vector<TexLoadSetting> m_objectTexLoadSettings
	{
		{L"Tex\\CyborgWeapon_BaseColor.dds", true},
		{L"Tex\\CyborgWeapon_Normal.dds"},
		{L"Tex\\CyborgWeapon_Metallic.dds", true},
		{L"Tex\\CyborgWeapon_Roughness.dds", true},
	};
	 
	static Matrix m_worldMatrix = Matrix::Identity;
	static float m_curRotationAngleRad = 0.f; 
	static const float m_rotationSpeed = 0.001f;
	 
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
		std::unique_ptr<DX12VertexBuffer> m_vertexBuffer = nullptr;
		std::vector<std::unique_ptr<DX12TextureResource>> m_texs{};
		std::vector<std::unique_ptr<D3D12_SAMPLER_DESC>> m_samplers{};
		std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>> m_vertexShaders;
		std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>> m_pixelShaders;
		std::vector<std::unique_ptr<DX12Shader>> m_computeShaders;
		std::unordered_map<UINT, std::shared_ptr<PipelineStateObject>> m_graphicsPipelineStates;

		std::unique_ptr<CameraManager> m_pCameraManager = nullptr;
		std::unique_ptr<LightManager> m_pLightManager = nullptr;
		std::unique_ptr<ShadowManager> m_pShadowManager = nullptr;
		std::unique_ptr<BufferManager> m_pBufferManager = nullptr;
		std::unique_ptr<RenderResource> m_pRenderSource = nullptr;
		
		std::unique_ptr<PipelineResourceSpace> m_perObjectBindResourceSpace = nullptr;
		std::unique_ptr<PipelineResourceSpace> m_perMainPassBindResourceSpace = nullptr;
		
		/// <summary>
		/// Model
		/// </summary>
		std::vector<DX12Model> m_models{};
		std::vector<DX12Vertex> m_vertices{};
		std::vector<UINT> m_indices{};
		std::vector<std::unique_ptr<DX12MeshRender>> m_meshRenders{};

		void UpdateCBV();
		void UpdatePassCBV();
		void UpdateObjectCBV();

		void InitTexTriangle();
		void LoadShaders();
		void LoadVertexIndexBuffer();
		void LoadConstantBuffers();
		void CreateCreamDepthRT();
		void LoadAndCreateTexs();
		void CreatePOS();

		void LoadModel();
		void AddShader(ShaderQueue shaderQueue, const std::wstring& shaderName, const std::wstring& entryPoint, ShaderType shaderType);
		void AddVertexBuffer(UINT singVertexSize, BufferAccessFlags bufferAccessFlag = BufferAccessFlags::HostWritable, bool isRawAccess = false);
	
		void RenderTexTriangle();

		void AddUIItems();
		void DrawCommand(size_t drawModelIndex);
		void DrawShadow();
		void DrawOpaque();
		void DrawSkybox();
};
}

   