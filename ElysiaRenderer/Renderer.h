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
		std::shared_ptr<DX12TextureResource> m_depthBuffer = nullptr;
		std::unique_ptr<DX12GraphicsContext> m_graphicsContext = nullptr;
		std::unique_ptr<DX12VertexBuffer> m_vertexBuffer = nullptr;
		std::vector<std::unique_ptr<DX12TextureResource>> m_texs{};
		std::vector<std::unique_ptr<D3D12_SAMPLER_DESC>> m_samplers;
		std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>> m_vertexShaders;
		std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>> m_pixelShaders;
		std::vector<std::unique_ptr<DX12Shader>> m_computeShaders;
		std::unordered_map<UINT, std::shared_ptr<PipelineStateObject>> m_graphicsPipelineStates;
		std::unordered_map<std::string, TexCreateDesc> m_depthBufferCreateDesc
		{
			{"Camera", {}},
			{"Shadow", {}},
		};

		std::unique_ptr<CameraManager> m_cameraManager = nullptr;
		std::unique_ptr<LightManager> m_lightManager = nullptr;
		std::unique_ptr<ShadowManager> m_shadowManager = nullptr;
		std::unique_ptr<BufferManager> m_bufferManager = nullptr;

		/// <summary>
		/// Constant parameter
		/// </summary>   
		struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)CBVMainPassParameter
		{
			Vector4 cameraPosWS = Vector4::Zero;	// 16
			Matrix viewMatrix = Matrix::Identity;	// 64
			Matrix projMatrix = Matrix::Identity; 	// 64
			Vector4 screenSize = Vector4::Zero;	// 16

			LightData mainLight;	// 64

			UINT frameIndex = 0;
			float nearZ = 1;
			float farZ = 1000;
		};
		struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)CBVShadowPassParameter
		{
			XMFLOAT4X4 viewMatrix = MathHelper::Identity4x4();	// 64
			XMFLOAT4X4 projMatrix = MathHelper::Identity4x4(); 	// 64

			XMFLOAT4X4 shadowMatrix = MathHelper::Identity4x4();	// 64

			float nearZ = 1;
			float farZ = 1000;
		};
		struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) CBVObjectParameter
		{
			UINT baseColorTexIndex;
			UINT normalTexIndex;
			UINT metallicTexIndex;
			UINT roughnessTexIndex;

			Matrix	worldMatrix = Matrix::Identity;

			Vector3	baseColorTint = Vector3::One;
			float		opacity = 1.f;

			float		normalIntensity = 1.f;
			float		metallicIntensity = 1.f;
			float		roughnessIntensity = 1.f;
			float		ambientCubemapIntensity = 1.f;

			Vector3	ambientCubemapTint = Vector3::One;

			//float padding[48];
		};
		CBVMainPassParameter m_mainPassParameter{};
		std::unique_ptr<DX12ConstantBuffer> m_passConstanBuffers = nullptr;
		std::unique_ptr<PipelineResourceSpace> m_perObjectBindResourceSpace = nullptr;
		//std::shared_ptr<PipelineResourceSpace> m_perShadowBindResourceSpace{};
		std::unique_ptr<PipelineResourceSpace> m_perMainPassBindResourceSpace = nullptr;
		 
		/// <summary>
		/// Model
		/// </summary>
		std::vector<DX12Model> m_models{};
		std::vector<DX12Vertex> m_vertices{};
		std::vector<UINT> m_indices{};
		std::vector<std::unique_ptr<DX12MeshRender>> m_meshRenders{};
		UINT m_objectCBVIndex = 0;

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

   