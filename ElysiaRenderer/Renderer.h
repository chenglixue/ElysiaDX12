#pragma once
#include "stdafx.h"
#include "DX12Device.h"
#include <dxgidebug.h>

namespace ElysiaRenderer
{
	using namespace DirectX;

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
		
		void InitTriangle();
		void InitTexTriangle();

		void RenderClearColor();
		void RenderTriangle();
		void RenderTexTriangle();

		float m_aspectRatio;
		std::unique_ptr<DX12Device> m_device = nullptr;
		std::unique_ptr<DX12GraphicsContext> m_graphicsContext = nullptr;
		std::unique_ptr<std::vector<DX12VertexBuffer>> m_vertexBuffer = nullptr;
		std::unique_ptr<std::vector<DX12RootParameter>> m_rootParameters;
		std::unique_ptr<std::vector<D3D12_SAMPLER_DESC>> m_samplers;
		std::unique_ptr<std::vector<DX12RootSignature>> m_rootSignatures = nullptr;
		std::unique_ptr<std::vector<DX12Shader>> m_vertexShader = nullptr;
		std::unique_ptr<std::vector<DX12Shader>> m_pixelShader = nullptr;
		std::unique_ptr<std::vector<DX12Shader>> m_computeShader = nullptr;
		std::unique_ptr<std::vector<DX12GraphicsPipelineState>> m_graphicsPipelineState = nullptr;
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
		InitTexTriangle();
	}
	inline void Renderer::Update()
	{

	}
	inline void Renderer::Render()
	{
		RenderTexTriangle();
	}
	inline void Renderer::Destory()
	{
		m_device->WaitForIdle();
		m_device->DestoryContext(m_graphicsContext.get());
		for (size_t i = 0; i < m_vertexBuffer.get()->size(); ++i)
		{
			m_device->DestoryBuffer(m_vertexBuffer.get()->data());
		}
		for (size_t i = 0; i < m_graphicsPipelineState.get()->size(); ++i)
		{
			m_device->DestoryPipelineState(m_graphicsPipelineState.get()->data());
		}
		for (size_t i = 0; i < m_vertexShader.get()->size(); ++i)
		{
			m_device->DestoryShader(m_vertexShader.get()->data());
		}
		for (size_t i = 0; i < m_pixelShader.get()->size(); ++i)
		{
			m_device->DestoryShader(m_pixelShader.get()->data());
		}
		m_device = nullptr;

		m_rootParameters.release();
		m_graphicsContext.release();
		m_vertexBuffer.release();
		m_rootSignatures.release();
		m_vertexShader.release();
		m_pixelShader.release();
		m_computeShader.release();
		m_graphicsPipelineState.release();

		if (IDXGIDebug* dxgiDebug = nullptr)
		{
			if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
			{
				dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
				dxgiDebug->Release();
			}
		}
	}

	void Renderer::InitTriangle()
	{
		struct TriangleVertex
		{
			XMFLOAT3 position;
			XMFLOAT4 color;
		};

		std::array<TriangleVertex, 3> triangleVertices;
		triangleVertices[0].position = { 0, 0.5, 0.f };
		triangleVertices[0].color = { 1, 0, 0, 1.f };
		triangleVertices[1].position = { 0.5f, -0.5f, 0.f };
		triangleVertices[1].color = { 0, 1, 0, 1.f };
		triangleVertices[2].position = { -0.5f, -0.5f, 0.f };
		triangleVertices[2].color = { 0, 0, 1, 1.f };

		VertexBufferCreationDesc vertexBufferCreationDesc{};
		vertexBufferCreationDesc.m_stride = sizeof(TriangleVertex);
		vertexBufferCreationDesc.m_size = sizeof(triangleVertices);
		vertexBufferCreationDesc.bufferAccessFlags = BufferAccessFlags::HostWritable;
		vertexBufferCreationDesc.bufferTypeFlags = BufferTypeFlags::SRV;
		vertexBufferCreationDesc.m_isRawAccess = false;

		auto vertexBuffer = m_device->CreateVertexBuffer(vertexBufferCreationDesc);
		vertexBuffer.SetMappedData(&triangleVertices, sizeof(triangleVertices));
		m_vertexBuffer->push_back(std::move(vertexBuffer));

		//m_rootSignature = std::move(m_device->CreateRootSignature());

		// Define the vertex input layout.
		std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		ShaderCreateDesc VSShaderCreateDesc{};
		VSShaderCreateDesc.shaderName = L"DrawTriangle.hlsl";
		VSShaderCreateDesc.entryPoint = "VS";
		VSShaderCreateDesc.shaderType = ShaderType::Vertex;
		m_vertexShader->push_back(std::move(m_device->CreateShader(VSShaderCreateDesc)));

		ShaderCreateDesc PSShaderCreateDesc{};
		PSShaderCreateDesc.shaderName = L"DrawTriangle.hlsl";
		PSShaderCreateDesc.entryPoint = "PS";
		PSShaderCreateDesc.shaderType = ShaderType::Pixel;
		m_pixelShader->push_back(std::move(m_device->CreateShader(PSShaderCreateDesc)));

		PipelineStateCreateDesc pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
		pipelineStateCreateDesc.m_vertexShader = &m_vertexShader->back();
		pipelineStateCreateDesc.m_pixelShader = &m_pixelShader->back();
		pipelineStateCreateDesc.m_rootSignature = &m_rootSignatures->back();
		pipelineStateCreateDesc.m_inputElementDesc = inputElementDescs;
		pipelineStateCreateDesc.m_renderTargetDesc.m_renderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		m_graphicsPipelineState->push_back(std::move(m_device->CreateGraphicsPipelineState(pipelineStateCreateDesc)));
	}
	void Renderer::InitTexTriangle()
	{
		struct TriangleVertex
		{
			XMFLOAT3 position;
			XMFLOAT2 uv;
		};

		std::array<TriangleVertex, 3> triangleVertices;
		triangleVertices[0].position = { 0, 0.5, 0.f };
		triangleVertices[0].uv = { 0.5, 0 };
		triangleVertices[1].position = { 0.5f, -0.5f, 0.f };
		triangleVertices[1].uv = { 1, 1 };
		triangleVertices[2].position = { -0.5f, -0.5f, 0.f };
		triangleVertices[2].uv = { 0, 1 };

		std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// Create Vertex Buffer
		{
			VertexBufferCreationDesc vertexBufferCreationDesc{};
			vertexBufferCreationDesc.m_stride = sizeof(TriangleVertex);
			vertexBufferCreationDesc.m_size = sizeof(triangleVertices);
			vertexBufferCreationDesc.bufferAccessFlags = BufferAccessFlags::HostWritable;
			vertexBufferCreationDesc.bufferTypeFlags = BufferTypeFlags::SRV;
			vertexBufferCreationDesc.m_isRawAccess = false;

			auto vertexBuffer = m_device->CreateVertexBuffer(vertexBufferCreationDesc);
			vertexBuffer.SetMappedData(&triangleVertices, sizeof(triangleVertices));
			m_vertexBuffer->push_back(std::move(vertexBuffer));
		}

		// Create Root Parameter & Sampler & Root Signature
		{
			{
				auto rootParameter = DX12RootParameter();
				rootParameter.InitAsDescriptorTable(1, D3D12_SHADER_VISIBILITY_PIXEL);
				rootParameter.SetTableRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

				m_rootParameters->push_back(std::move(rootParameter));
			}

			RootSignatureCreatDesc rootSignatureCreatDesc;
			rootSignatureCreatDesc.rootParamters = *m_rootParameters.get();

			m_rootSignatures->push_back(std::move(m_device->CreateRootSignature(rootSignatureCreatDesc)));
		}

		// Create Shader
		{
			ShaderCreateDesc VSShaderCreateDesc{};
			VSShaderCreateDesc.shaderName = L"DrawTriangle.hlsl";
			VSShaderCreateDesc.entryPoint = "VS";
			VSShaderCreateDesc.shaderType = ShaderType::Vertex;
			m_vertexShader->push_back(std::move(m_device->CreateShader(VSShaderCreateDesc)));

			ShaderCreateDesc PSShaderCreateDesc{};
			PSShaderCreateDesc.shaderName = L"DrawTriangle.hlsl";
			PSShaderCreateDesc.entryPoint = "PS";
			PSShaderCreateDesc.shaderType = ShaderType::Pixel;
			m_pixelShader->push_back(std::move(m_device->CreateShader(PSShaderCreateDesc)));
		}

		// Create PSO
		{
			PipelineStateCreateDesc pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
			pipelineStateCreateDesc.m_vertexShader = &m_vertexShader->back();
			pipelineStateCreateDesc.m_pixelShader = &m_pixelShader->back();
			pipelineStateCreateDesc.m_rootSignature = &m_rootSignatures->back();
			pipelineStateCreateDesc.m_inputElementDesc = inputElementDescs;
			pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = 1;
			pipelineStateCreateDesc.m_renderTargetDesc.m_renderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

			m_graphicsPipelineState->push_back(std::move(m_device->CreateGraphicsPipelineState(pipelineStateCreateDesc)));
		}
	}

	void Renderer::RenderClearColor()
	{
		m_device->BeginFrame();

		auto& currBackBuffer = m_device->GetCurrBackBuffer();

		m_graphicsContext->Reset();
		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_graphicsContext->FlushBarrier();

		m_graphicsContext->ClearRenderTarget(currBackBuffer, Color(0.3, 0.3, 0.8));

		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_PRESENT);
		m_graphicsContext->FlushBarrier();

		m_device->SubmitContextWork(m_graphicsContext.get());

		m_device->Present();
		m_device->EndFrame();
	}
	void Renderer::RenderTriangle()
	{
		m_device->BeginFrame();

		auto& currBackBuffer = m_device->GetCurrBackBuffer();

		size_t pipelineStateIndex = 0;
		size_t vertexBufferIndex = 0;
		m_graphicsContext->Reset((*m_graphicsPipelineState)[pipelineStateIndex].GetPipelineState());
		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_graphicsContext->FlushBarrier();

		m_graphicsContext->ClearRenderTarget(currBackBuffer, Color(0., 0., 0.));

		m_graphicsContext->SetVertexBuffer(0, 1, (*m_vertexBuffer)[vertexBufferIndex].GetVertexBufferView());
		auto pipelineStateData = CreatePipelineStateData(&(*m_graphicsPipelineState)[pipelineStateIndex],
			std::move(std::vector<DX12TextureResource*>{ &currBackBuffer }));
		m_graphicsContext->SetPipeline(pipelineStateData);
		m_graphicsContext->SetDefaultViewportAndScissor(m_device->GetScreenSize());
		m_graphicsContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_graphicsContext->Draw(3, 0);

		m_graphicsContext->AddBarrier(currBackBuffer, D3D12_RESOURCE_STATE_PRESENT);
		m_graphicsContext->FlushBarrier();

		m_device->SubmitContextWork(m_graphicsContext.get());

		m_device->Present();
		m_device->EndFrame();
	}
	void Renderer::RenderTexTriangle()
	{

	}
}