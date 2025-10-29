#include "stdafx.h"
#include "OpaquePass.h"
#include "DX12Device.h"
#include "RenderTexture.h"


namespace ElysiaRenderer
{
	OpaquePass::~OpaquePass()
	{
		Dispose();
	}

	void OpaquePass::Configure()
	{
		AddShader(ShaderQueue::Opaque, L"Shaders\\public\\Opaque.hlsl", L"VS", ShaderType::Vertex);
		AddShader(ShaderQueue::Opaque, L"Shaders\\public\\Opaque.hlsl", L"PS", ShaderType::Pixel);

		CreateRTs();
		BindToShader();
		CreatePSO();
	}

	void OpaquePass::Dispose()
	{

	}

	void OpaquePass::Execute()
	{

	}
	void OpaquePass::Render()
	{
		Execute();

		m_pCommand->AddBarrier(*m_pOpaqueRT->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_pCommand->FlushBarrier();
		m_pCommand->ClearRenderTarget(*m_pOpaqueRT->GetTexture(), Color(0, 0, 0));

		m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize.x, m_renderSize.y));

		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = (*m_pGraphicsPipelineStates)[ShaderQueue::Opaque].get();
		pipelineStateData.m_renderTargets = { m_pOpaqueRT->GetTexture()};
		pipelineStateData.m_depthStencilTarget = GetBufferManager()->GetCameraDepthRT()->GetTexture();

		bool isReady = true;
		{
			if (m_pOpaqueRT->GetTexture() == nullptr || GetBufferManager()->GetCameraDepthRT()->GetTexture() == nullptr)
			{
				ThrowRuntimeError("null texture resource");
			}
			isReady &= m_pOpaqueRT->GetTexture()->GetIsReady();
			isReady &= GetBufferManager()->GetCameraDepthRT()->GetTexture()->GetIsReady();
		}
		if (isReady)
		{
			m_pCommand->SetPipeline(pipelineStateData);
			m_pCommand->SetPipelineResource(PER_PASS_SPACE, RenderResource::GetPerMainBindResourceSpace());

			UINT vertexStride = GetModelImporter()->GetVertexStride();

			for (UINT meshIndex = 0; meshIndex < GetModelImporter()->GetMeshCount(); ++meshIndex)
			{
				const auto& meshRenderer = GetModelImporter()->GetMeshRenderer(meshIndex);
				const auto& mesh = meshRenderer.m_mesh;

				auto objectContantBuffer = GetBufferManager()->GetMutilConstantBuffer(PER_OBJECT_SPACE, GetDevice()->GetFrameID(), meshIndex);
				RenderResource::GetPerObjectBindResourceSpace()->SetCBV(objectContantBuffer);
				m_pCommand->SetPipelineResource(PER_OBJECT_SPACE, RenderResource::GetPerObjectBindResourceSpace());

				auto startIndex = mesh->indexDataOffset / sizeof(UINT16);
				auto startVertex = mesh->vertexDataOffset / vertexStride;
				auto VertexCount = mesh->vertexCount;
				auto indexCount = mesh->indexCount;

				m_pCommand->Draw(indexCount, startVertex, startIndex);
			}
		}

		m_pCommand->AddBarrier(*m_pOpaqueRT->GetTexture(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_pCommand->FlushBarrier();
	}

	void OpaquePass::CreateRTs()
	{
		m_pOpaqueRT = CreateRenderTexture(static_cast<UINT64>(m_renderSize.x), 
			static_cast<UINT64>(m_renderSize.y),
			DXGI_FORMAT_R8G8B8A8_UNORM,
			L"Opaque Lighting RT");
	}
	void OpaquePass::BindToShader()
	{

	}
	void OpaquePass::CreatePSO()
	{
		PipelineStateCreateDesc pipelineStateCreateDesc{};
		PipelineResourceLayout meshResourceLayout{};

		meshResourceLayout.m_spaces[PER_OBJECT_SPACE] = RenderResource::GetPerObjectBindResourceSpace();
		meshResourceLayout.m_spaces[PER_PASS_SPACE] = RenderResource::GetPerMainBindResourceSpace();

		pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
		pipelineStateCreateDesc.m_vertexShader = GetVertexShaders()[ShaderQueue::GBuffer][ShaderType::Vertex].get();
		pipelineStateCreateDesc.m_pixelShader = GetPixelShaders()[ShaderQueue::GBuffer][ShaderType::Pixel].get();
		pipelineStateCreateDesc.m_inputElementDesc = g_inputElementDescs;
		pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = 1;
		pipelineStateCreateDesc.m_renderTargetDesc.m_renderTargetFormats[0] = m_pOpaqueRT->GetFormat();
		pipelineStateCreateDesc.m_renderTargetDesc.m_depthStencilFormat = GetBufferManager()->GetCameraDepthRT()->GetFormat();
		pipelineStateCreateDesc.m_depthStencilDesc = GetDepthState(DepthState::Enabled);
		pipelineStateCreateDesc.m_blendDesc = GetBlendState(BlendState::Disabled);
		pipelineStateCreateDesc.m_rasterDesc = GetRasterizerState(RasterizerState::NoCullNoMS);
		pipelineStateCreateDesc.m_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		(*m_pGraphicsPipelineStates)[ShaderQueue::Opaque] = std::move(GetDevice()->CreateGraphicsPipelineState(pipelineStateCreateDesc, meshResourceLayout));
	}
}