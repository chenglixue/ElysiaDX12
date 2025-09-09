#pragma once
#include "stdafx.h"
#include "DX12Device.h"
#include "DX12MeshRender.h"
#include "DX12Camera.h"
#include "DX12Light.h"
#include "DX12UI.h"
#include <dxgidebug.h>

namespace ElysiaRenderer 
{
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
		L"Mesh\\Sphere.fbx",
	};
	const std::vector<LPCWSTR> m_globalTexPaths
	{
		L"Tex\\GGX_E_LUT.dds",
		L"Tex\\GGX_Eavg_LUT.dds",
		L"Tex\\cubemap0.dds",
	};
	const std::vector<LPCWSTR> m_objectTexPaths
	{
		L"Tex\\CyborgWeapon_BaseColor.dds",
		L"Tex\\CyborgWeapon_Normal.dds",
		L"Tex\\CyborgWeapon_Metallic.dds",
		L"Tex\\CyborgWeapon_Roughness.dds",
	};

	static XMMATRIX m_worldMatrix = XMMatrixIdentity();
	static float m_curRotationAngleRad = 0.f; 
	static const float m_rotationSpeed = 0.001f;
	static int objectNum = 3;    
	 
	/// <summary>
	/// Constant parameter
	/// </summary>  
	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) CBVPassParameter
	{
		XMFLOAT4 cameraPosWS;	// 16
		XMMATRIX viewMatrix;	// 64
		XMMATRIX projMatrix;	// 64
		XMFLOAT4 screenSize;	// 16

		LightData mainLights[MAX_MAIN_LIGHT_COUNT];	// 64
		//float padd[24];

		UINT frameIndex;
	};
	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) CBVObjectParameter
	{
		XMMATRIX	worldMatrix;

		XMFLOAT3	baseColorTint = XMFLOAT3(1.f, 1.f, 1.f);
		float		opacity = 1.f;

		float		normalIntensity = 1.f;
		float		metallicIntensity = 1.f;
		float		roughnessIntensity = 1.f;
		float		ambientCubemapIntensity = 1.f;

		XMFLOAT3	ambientCubemapTint = XMFLOAT3(1.f, 1.f, 1.f);

		//float padding[48];
	};
	static CBVPassParameter m_passParameter{};
	static std::vector<CBVObjectParameter> m_objectParameters{}; 

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

		virtual void OnMouseDown(WPARAM btnState, int x, int y);
		virtual void OnMouseUp(WPARAM btnState, int x, int y);
		virtual void OnMouseMove(WPARAM btnState, int x, int y);
		virtual void OnKeyboardInput();

	protected:
		Renderer* m_render = nullptr;
		HWND m_windowHandle;

		bool m_isStopped = false;
		bool m_isMin = false;
		bool m_isMax = false;
		bool m_isResizing = false;

		XMINT2 m_lastMousePos;

		/// <summary>
		/// pipeline
		/// </summary>
		float m_aspectRatio;
		std::shared_ptr<DX12UI> m_pUI = nullptr;
		std::unique_ptr<DX12Device> m_device = nullptr;
		std::unique_ptr<DX12TextureResource> m_depthBuffer = nullptr;
		std::unique_ptr<DX12GraphicsContext> m_graphicsContext = nullptr;
		std::unique_ptr<DX12VertexBuffer> m_vertexBuffer = nullptr;
		std::unique_ptr<DX12IndexBuffer> m_indexBuffer = nullptr;
		std::vector<std::unique_ptr<DX12RootParameter>> m_rootParameters;
		std::vector<std::unique_ptr<D3D12_SAMPLER_DESC>> m_samplers;
		std::vector<std::unique_ptr<DX12RootSignature>> m_rootSignatures;
		std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>> m_vertexShaders;
		std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>> m_pixelShaders;
		std::vector<std::unique_ptr<DX12Shader>> m_computeShaders;
		std::unordered_map<UINT, std::unique_ptr<DX12GraphicsPipelineState>> m_graphicsPipelineStates;
		std::vector<std::shared_ptr<DX12Camera>> m_cameras;
		std::vector<std::unique_ptr<DX12Light>> m_lights;
		PipelineBindResource m_pipelineBindResource{};
		TexCreateDesc m_depthBufferCreateDesc{};

		std::shared_ptr<DX12Camera> m_mainCamera;

		/// <summary>
		/// Model
		/// </summary>
		std::vector<DX12Model> m_models{};
		std::vector<DX12Vertex> m_vertices{};
		std::vector<UINT> m_indices{};
		std::vector<std::unique_ptr<DX12MeshRender>> m_meshRenders{};

		void InitTexTriangle();
		void RenderTexTriangle();

		void LoadShaders();
		void LoadVertexIndexBuffer();
		void LoadConstantBuffers();
		void CreateCreamDepthRT();
		void LoadAndCreateTexs();
		void CreateSignatures();
		void CreatePOS();

		std::shared_ptr<DX12Camera> InitCamera(XMVECTOR position, float aspect, float FOVY, float nearZ, float farZ);
		void InitLight();
		void LoadModel();
		void BindObject(DX12TextureResource& currBackBuffer, 
			UINT& objectCBVIndex, uint8_t pipelineStateQueue, size_t drawMeshIndex);
		void AddUIItems();
		void AddShader(ShaderQueue shaderQueue, const std::wstring& shaderName, const std::wstring& entryPoint, ShaderType shaderType);
		void AddVertexBuffer(UINT singVertexSize, BufferAccessFlags bufferAccessFlag = BufferAccessFlags::HostWritable, bool isRawAccess = false);
		void AddIndexBuffer(UINT singIndexSize, DXGI_FORMAT format, BufferAccessFlags bufferAccessFlag = BufferAccessFlags::HostWritable);
	};
}

