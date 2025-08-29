#pragma once
#include "stdafx.h"
#include "DX12Device.h"
#include "DX12MeshRender.h"
#include "DX12Camera.h"
#include "DX12Light.h"
#include <dxgidebug.h>

namespace ElysiaRenderer
{
	class Renderer
	{
	public:
		Renderer(HWND windowHandle, ElysiaHelper::UINT2 screenSize);
		~Renderer();

		void Init();
		void Update();
		void Render();
		void Destory();

	private:

		/// <summary>
		/// pipeline
		/// </summary>
		float m_aspectRatio;
		const std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputElementDescs =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
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
		std::vector<std::unique_ptr<DX12Camera>> m_cameras;
		std::vector<std::unique_ptr<DX12Light>> m_lights;
		PipelineBindResource m_pipelineBindResource;

		/// <summary>
		/// Constant parameter
		/// </summary>
		struct CBVPassParameter
		{
			XMFLOAT4 cameraPosWS;	// 16
			XMFLOAT4X4 viewMatrix;	// 64
			XMFLOAT4X4 projMatrix;	// 64
			XMFLOAT4 screenSize;	// 16

			LightData lights[1];	// 64
			XMFLOAT4 padd[2];
		};
		struct CBVObjectParameter
		{
			XMFLOAT4X4 worldMatrix;
			
			XMFLOAT4 padding[12];
		};
		static_assert((sizeof(CBVPassParameter) % 256) == 0, "Constant Buffer size must be 256-byte aligned");
		static_assert((sizeof(CBVObjectParameter) % 256) == 0, "Constant Buffer size must be 256-byte aligned");
		CBVPassParameter m_passParameter;
		std::vector<CBVObjectParameter> m_objectParameters{};

		/// <summary>
		/// Model
		/// </summary>
		std::vector<DX12Model> m_models{};
		std::vector<DX12Vertex> m_vertices{};
		std::vector<UINT> m_indices{};
		std::vector<std::unique_ptr<DX12MeshRender>> m_meshRenders{};

		/// <summary>
		/// user data
		/// </summary>
		XMMATRIX m_worldMatrix = XMMatrixIdentity();
		XMMATRIX m_viewMatrix = XMMatrixIdentity();
		XMMATRIX m_projMatrix = XMMatrixIdentity();
		float m_curRotationAngleRad = 0.f;
		const float m_rotationSpeed = 0.01f;

		const std::vector<LPCWSTR> m_modelPaths
		{
			//L"Mesh\\LOW_WEPON.fbx",
			L"Mesh\\Sphere.fbx",
		};
		const std::vector<LPCWSTR> m_globalTexPaths
		{
			L"Tex\\GGX_E_LUT.dds",
			L"Tex\\GGX_Eavg_LUT.dds",
			L"Tex\\Skybox.dds",
		};
		const std::vector<LPCWSTR> m_objectTexPaths
		{
			L"Tex\\CyborgWeapon_BaseColor.dds",
			L"Tex\\CyborgWeapon_Normal.dds",
			L"Tex\\CyborgWeapon_Metallic.dds",
			L"Tex\\CyborgWeapon_Roughness.dds",
		};

		void InitTexTriangle();

		void RenderTexTriangle();

		void InitLight();
		void LoadModel();
		void BindObject(DX12TextureResource& currBackBuffer, 
			size_t& objectCBVIndex, UINT pipelineStateQueue, size_t drawMeshIndex,
			const CBVObjectParameter& tempCBVObjectParameter);
		void AddShader(ShaderQueue shaderQueue, const std::wstring& shaderName, const std::wstring& entryPoint, ShaderType shaderType);
		void AddVertexBuffer(UINT singVertexSize, BufferAccessFlags bufferAccessFlag, bool isRawAccess);
		void AddIndexBuffer(UINT singIndexSize, DXGI_FORMAT format, BufferAccessFlags bufferAccessFlag);
	};

	Renderer::Renderer(HWND windowHandle, ElysiaHelper::UINT2 screenSize)
	{
		m_aspectRatio = static_cast<float>(screenSize.x) / static_cast<float>(screenSize.y);

		m_device = std::make_unique<DX12Device>(windowHandle, screenSize);
		m_graphicsContext = m_device->CreateGraphicsContext();

	}

	Renderer::~Renderer()
	{

	}

	inline void Renderer::Init()
	{
		m_passParameter = CBVPassParameter();

		{
			auto camera = std::make_unique<DX12Camera>();
			camera->SetCameraPos({ 0.0f, 5.0f, 10.0f });
			camera->SetCameraFOV(0.8f);
			camera->SetCameraNearZ(0.01f);
			camera->SetCameraFarz(100.f);
			static const XMVECTORF32 up{ 0.f, 1.f, 0.f, 0.f };
			static const XMVECTORF32 at{ 0.f, 0.f, 0.f, 0.f };
			m_viewMatrix = XMMatrixLookAtLH(camera->GetCameraPos(), at, up);

			m_projMatrix = XMMatrixPerspectiveFovLH(camera->GetFOV(), m_aspectRatio, camera->GetNearZ(), camera->GetFarZ());

			XMStoreFloat4x4(&m_passParameter.viewMatrix, (m_viewMatrix));
			XMStoreFloat4x4(&m_passParameter.projMatrix, (m_projMatrix));
			XMStoreFloat4(&m_passParameter.cameraPosWS, camera->GetCameraPos());

			m_cameras.emplace_back(std::move(camera));
		}

		LoadModel();
		InitLight();
		InitTexTriangle();
	}
	inline void Renderer::Update()
	{
		m_curRotationAngleRad += m_rotationSpeed;
		m_worldMatrix = XMMatrixRotationZ(0);

		m_passParameter.screenSize = m_device->GetScreenSize();
		m_pipelineBindResource.m_CBVResource[ElysiaHelper::PER_PASS_SPACE][0]->SetMappedData(&m_passParameter, sizeof(CBVPassParameter));
	}
	inline void Renderer::Render()
	{
		RenderTexTriangle();
	}
	inline void Renderer::Destory()
	{
		m_device->WaitForIdle();
		m_device->DestoryContext(std::move(m_graphicsContext));
		m_device->DestoryBuffer(std::move(m_vertexBuffer));
		for (size_t i = 0; i < m_graphicsPipelineStates.size(); ++i)
		{
			m_device->DestoryPipelineState(std::move(m_graphicsPipelineStates[i]));
		}
		m_device = nullptr;

		m_rootParameters.clear();
		m_graphicsContext.release();
		m_vertexBuffer.release();
		m_rootSignatures.clear();
		m_vertexShaders.clear();
		m_pixelShaders.clear();
		m_computeShaders.clear();
		m_graphicsPipelineStates.clear();
	}
	inline void Renderer::InitLight()
	{
		auto dirLight = std::make_unique<DX12DirectionLight>(XMFLOAT4(1, 1, 1, 1), XMFLOAT4(0, 0, -1, 0), 1);
		m_passParameter.lights[0] = std::move(dirLight->CreateLightData());

		m_lights.emplace_back(std::move(dirLight));
	}
	inline void Renderer::LoadModel()
	{
		for (size_t currModelIndex = 0; currModelIndex < m_modelPaths.size(); ++currModelIndex)
		{
			auto currModel = DX12Model(m_modelPaths[currModelIndex]);
			auto currVertices = std::move(currModel.GetVertices());
			auto currIndices = std::move(currModel.GetIndices());

			m_vertices.insert(m_vertices.end(), currVertices.begin(), currVertices.end());
			m_indices.insert(m_indices.end(), currIndices.begin(), currIndices.end());

			m_models.emplace_back(std::move(currModel));
		}
	}
	inline void Renderer::AddShader(ShaderQueue shaderQueue, const std::wstring& shaderName, const std::wstring& entryPoint, ShaderType shaderType)
	{
		ShaderCreateDesc VSShaderCreateDesc{};
		VSShaderCreateDesc.shaderName = shaderName;
		VSShaderCreateDesc.entryPoint = entryPoint;
		VSShaderCreateDesc.shaderType = shaderType;

		std::unique_ptr<DX12Shader> shader = std::move(m_device->CreateShader(VSShaderCreateDesc));
		std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>> value{};
		value[shaderType] = std::move(shader);

		switch (shaderType)
		{
		case ShaderType::Vertex:
		{
			m_vertexShaders[shaderQueue] = std::move(value);
			break;
		}
		{
		case ShaderType::Pixel:
			m_pixelShaders[shaderQueue] = std::move(value);
		}
		}
		
	}
	inline void Renderer::AddVertexBuffer(UINT singVertexSize, BufferAccessFlags bufferAccessFlag = BufferAccessFlags::HostWritable, bool isRawAccess = false)
	{
		VertexBufferCreationDesc vertexBufferCreationDesc{};
		vertexBufferCreationDesc.m_stride = singVertexSize;
		vertexBufferCreationDesc.m_size = static_cast<UINT>(m_vertices.size()) * vertexBufferCreationDesc.m_stride;
		vertexBufferCreationDesc.bufferAccessFlags = bufferAccessFlag;
		vertexBufferCreationDesc.bufferTypeFlags = BufferTypeFlags::None;
		vertexBufferCreationDesc.m_isRawAccess = isRawAccess;

		m_vertexBuffer = m_device->CreateVertexBuffer(vertexBufferCreationDesc);
		m_vertexBuffer->SetMappedData(m_vertices.data(), vertexBufferCreationDesc.m_size);
	}
	inline void Renderer::AddIndexBuffer(UINT singIndexSize, DXGI_FORMAT format, BufferAccessFlags bufferAccessFlag = BufferAccessFlags::HostWritable)
	{
		IndexBufferCreateDesc indexBufferCreationDesc{};
		indexBufferCreationDesc.m_bufferSize = static_cast<UINT>(m_indices.size()) * singIndexSize;
		indexBufferCreationDesc.m_format = format;
		indexBufferCreationDesc.m_vertexMappedBuffer = m_vertexBuffer->GetMappedBuffer();
		indexBufferCreationDesc.bufferTypeFlags = BufferTypeFlags::None;
		indexBufferCreationDesc.bufferAccessFlags = bufferAccessFlag;

		m_indexBuffer = m_device->CreateIndexBuffer(indexBufferCreationDesc);
		m_indexBuffer->SetMappedData(m_indices.data(), indexBufferCreationDesc.m_bufferSize);
	}
	
	inline void Renderer::BindObject(DX12TextureResource& currBackBuffer,
		size_t& objectCBVIndex, UINT pipelineStateQueue, size_t drawMeshIndex,
		const CBVObjectParameter& tempCBVObjectParameter)
	{
		// get object constant buffer
		auto objectConstanBuffer = dynamic_cast <DX12ConstantBuffer*>(m_pipelineBindResource.m_CBVResource[ElysiaHelper::PER_OBJECT_SPACE][objectCBVIndex].get());
		m_objectParameters.emplace_back(CBVObjectParameter());

		// set object constant data in buffer
		m_objectParameters.back() = tempCBVObjectParameter;
		objectConstanBuffer->SetMappedData(&m_objectParameters[objectCBVIndex], sizeof(CBVObjectParameter));
		m_pipelineBindResource.CBVSizes[ElysiaHelper::PER_OBJECT_SPACE] = sizeof(CBVObjectParameter);
		m_pipelineBindResource.CBVIndexs[ElysiaHelper::PER_OBJECT_SPACE] = objectCBVIndex;


		// set pipeline & bind data
		auto pipelineStateData = CreatePipelineStateData(m_graphicsPipelineStates[pipelineStateQueue].get(),
			std::move(std::vector<DX12TextureResource*>{ &currBackBuffer }),
			m_depthBuffer.get());
		m_graphicsContext->SetPipeline(pipelineStateData, m_pipelineBindResource);

		assert(drawMeshIndex < m_models.size());
		auto drawMeshs = m_models[drawMeshIndex].GetMeshs();

		for (int i = 0; i < drawMeshs.size(); ++i)
		{
			auto drawVertexCount = static_cast<UINT>(drawMeshs[i].m_indexCount);
			auto startVertex = drawMeshs[i].m_currStartVertex;
			auto startIndex = drawMeshs[i].m_currStartIndex;

			m_graphicsContext->Draw(drawVertexCount, startVertex, startIndex);
		}

	}


	void Renderer::InitTexTriangle()
	{
		// Create Shader
		{
			AddShader(ShaderQueue::Opaque, L"Shaders\\DrawTriangle.hlsl", L"VS", ShaderType::Vertex);
			AddShader(ShaderQueue::Opaque, L"Shaders\\DrawTriangle.hlsl", L"PS", ShaderType::Pixel);
			/*AddShader(ShaderQueue::Skybox, L"Shaders\\DrawTriangle.hlsl", L"PS", ShaderType::Pixel);
			AddShader(ShaderQueue::Skybox, L"Shaders\\DrawTriangle.hlsl", L"PS", ShaderType::Pixel);*/
		}

		// Create Vertex & Index Buffer
		{
			AddVertexBuffer(sizeof(DX12Vertex));
			AddIndexBuffer(sizeof(UINT), DXGI_FORMAT_R32_UINT);
		}

		// Create Constant Buffer
		{
			ConstantBufferCreationDesc constantBufferCreationDesc{};
			constantBufferCreationDesc.m_bufferSize = sizeof(CBVPassParameter);
			constantBufferCreationDesc.m_bufferIndex = 0;
			constantBufferCreationDesc.bufferAccessFlags = BufferAccessFlags::HostWritable;
			constantBufferCreationDesc.bufferTypeFlags = BufferTypeFlags::CBV;
			constantBufferCreationDesc.m_isRawAccess = false;

			auto constantBuffer = m_device->CreateConstantBuffer(constantBufferCreationDesc);
			constantBuffer->SetMappedData(&m_passParameter, sizeof(CBVPassParameter));
			m_pipelineBindResource.m_CBVResource[ElysiaHelper::PER_PASS_SPACE].push_back(std::move(constantBuffer));

			constantBuffer = m_device->CreateConstantBuffer(constantBufferCreationDesc);
			//constantBuffer->SetMappedData(&m_objectParameters[0], sizeof(CBVObjectParameter));
			m_pipelineBindResource.m_CBVResource[ElysiaHelper::PER_OBJECT_SPACE].push_back(std::move(constantBuffer));

			constantBuffer = m_device->CreateConstantBuffer(constantBufferCreationDesc);
			//constantBuffer->SetMappedData(&m_objectParameters[1], sizeof(CBVObjectParameter));
			m_pipelineBindResource.m_CBVResource[ElysiaHelper::PER_OBJECT_SPACE].push_back(std::move(constantBuffer));
		}

		// Create Depth Buffer
		TexCreateDesc depthBufferCreateDesc{};
		{
			depthBufferCreateDesc.m_resouceDesc.Format = DXGI_FORMAT_D32_FLOAT;
			depthBufferCreateDesc.m_resouceDesc.Width = m_device->GetScreenSize().x;
			depthBufferCreateDesc.m_resouceDesc.Height = m_device->GetScreenSize().y;
			depthBufferCreateDesc.m_typeFlag = TexTypeFlags::SRV | TexTypeFlags::DSV;

			m_depthBuffer = std::move(m_device->CreateTexture(depthBufferCreateDesc));
		}

		// Create Tex & Buffer
		{
			auto texPaths = m_globalTexPaths;
			for (int i = 0; i < texPaths.size(); ++i)
			{
				auto strPath = ElysiaHelper::LPCWSTRToString(texPaths[i]);

				TextureCreationDesc texBufferCreateDesc{};

				texBufferCreateDesc.texturePath = texPaths[i];
				texBufferCreateDesc.isSRGB = false;

				auto newTex = std::move(m_device->CreateTextureFromFile(texBufferCreateDesc));
				m_pipelineBindResource.m_SRVResources[ElysiaHelper::PER_PASS_SPACE].push_back(std::move(newTex));
			}

			texPaths = m_objectTexPaths;
			for (int i = 0; i < texPaths.size(); ++i)
			{
				auto strPath = ElysiaHelper::LPCWSTRToString(texPaths[i]);
				bool isSRGB = strPath.find("_BaseColor") == strPath.npos ? false : true;

				TextureCreationDesc texBufferCreateDesc{};

				texBufferCreateDesc.texturePath = texPaths[i];
				texBufferCreateDesc.isSRGB = isSRGB;

				auto newTex = std::move(m_device->CreateTextureFromFile(texBufferCreateDesc));
				m_pipelineBindResource.m_SRVResources[ElysiaHelper::PER_OBJECT_SPACE].push_back(std::move(newTex));
			}
			
		}

		// Create Root Parameter & Sampler & Root Signature
		{
			{
				auto rootParameter = std::make_unique<DX12RootParameter>();
				rootParameter->InitAsDescriptorTable(1, D3D12_SHADER_VISIBILITY_PIXEL);
				rootParameter->SetTableRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<UINT>(m_pipelineBindResource.m_SRVResources[ElysiaHelper::PER_PASS_SPACE].size()),
					0, 0, ElysiaHelper::PER_PASS_SPACE);
				m_rootParameters.push_back(std::move(rootParameter));

				rootParameter = std::make_unique<DX12RootParameter>();
				rootParameter->InitAsDescriptorTable(1, D3D12_SHADER_VISIBILITY_PIXEL);
				rootParameter->SetTableRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<UINT>(m_pipelineBindResource.m_SRVResources[ElysiaHelper::PER_OBJECT_SPACE].size()),
					0, 0, ElysiaHelper::PER_OBJECT_SPACE);
				m_rootParameters.push_back(std::move(rootParameter));

				rootParameter = std::make_unique<DX12RootParameter>();
				rootParameter->InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_ALL, ElysiaHelper::PER_PASS_SPACE);
				m_rootParameters.push_back(std::move(rootParameter));

				rootParameter = std::make_unique<DX12RootParameter>();
				rootParameter->InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_ALL, ElysiaHelper::PER_OBJECT_SPACE);
				m_rootParameters.push_back(std::move(rootParameter));
			}

			RootSignatureCreatDesc rootSignatureCreatDesc;
			std::vector<DX12RootParameter*> tempRootParameters{};
			for (size_t i = 0; i < m_rootParameters.size(); ++i)
			{
				tempRootParameters.push_back(m_rootParameters[i].get());
			}
			rootSignatureCreatDesc.rootParamters = std::move(tempRootParameters);

			m_rootSignatures.push_back(std::move(m_device->CreateRootSignature(rootSignatureCreatDesc)));
		}

		// Create PSO
		{
			PipelineStateCreateDesc pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
			pipelineStateCreateDesc.m_vertexShader = m_vertexShaders[ShaderQueue::Opaque][ShaderType::Vertex].get();
			pipelineStateCreateDesc.m_pixelShader = m_pixelShaders[ShaderQueue::Opaque][ShaderType::Pixel].get();
			pipelineStateCreateDesc.m_rootSignature = m_rootSignatures.back().get();
			pipelineStateCreateDesc.m_inputElementDesc = m_inputElementDescs;
			pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = 1;
			pipelineStateCreateDesc.m_renderTargetDesc.m_renderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			pipelineStateCreateDesc.m_depthStencilDesc.DepthEnable = TRUE;
			pipelineStateCreateDesc.m_renderTargetDesc.m_depthStencilFormat = depthBufferCreateDesc.m_resouceDesc.Format;
			pipelineStateCreateDesc.m_depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

			m_graphicsPipelineStates.insert({ ShaderQueue::Opaque, std::move(m_device->CreateGraphicsPipelineState(pipelineStateCreateDesc)) });
		}
	}
	void Renderer::RenderTexTriangle()
	{
		m_objectParameters.clear();

		m_device->BeginFrame();

		auto& currBackBuffer = m_device->GetCurrBackBuffer();
		size_t pipelineStateIndex = 0;
		size_t objectCBVIndex = 0;

		m_graphicsContext->Reset(m_graphicsPipelineStates[ShaderQueue::Opaque]->GetPipelineState());
		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_graphicsContext->AddBarrier(*m_depthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_graphicsContext->FlushBarrier();

		m_graphicsContext->ClearRenderTarget(currBackBuffer, Color(1, 1, 1));
		m_graphicsContext->ClearDepthStencilTarget(*m_depthBuffer, 1.f, 0);

		m_graphicsContext->SetVertexBuffer(0, 1, m_vertexBuffer->GetVertexBufferView());
		m_graphicsContext->SetIndexBuffer(m_indexBuffer->GetIndexBufferView());

		m_graphicsContext->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(static_cast<UINT>(m_device->GetScreenSize().x), static_cast<UINT>(m_device->GetScreenSize().y)));
		m_graphicsContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		{
			CBVObjectParameter tempCBVObjectParameter{};
			XMMATRIX scaleMatrix = XMMatrixScaling(1.f, 1.f, 1.f);
			XMMATRIX rotationMatrix = XMMatrixRotationAxis(XMVectorSet(0, 1, 0, 0), 45) * XMMatrixRotationAxis(XMVectorSet(0, 0, 1, 0), 55);
			/*XMStoreFloat4x4(&tempCBVObjectParameter.worldMatrix, m_worldMatrix * rotationMatrix * scaleMatrix);
			BindObject(currBackBuffer, objectCBVIndex, pipelineStateIndex, 0, tempCBVObjectParameter);
			objectCBVIndex++;*/

			tempCBVObjectParameter = {};
			scaleMatrix = XMMatrixScaling(1.f, 1.f, 1.f);
			XMMATRIX translateMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
			XMStoreFloat4x4(&tempCBVObjectParameter.worldMatrix, m_worldMatrix * scaleMatrix);
			BindObject(currBackBuffer, objectCBVIndex, ShaderQueue::Opaque, 0, tempCBVObjectParameter);
		}

		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_PRESENT);
		m_graphicsContext->FlushBarrier();

		m_device->SubmitContextWork(*m_graphicsContext);

		m_device->EndFrame();
		m_device->Present();
	}


}