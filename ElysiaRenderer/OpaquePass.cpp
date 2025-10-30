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
		AddShader(ShaderQueue::Blit, L"Shaders\\public\\FullScreenTriangle.hlsl", L"VS", ShaderType::Vertex);
		AddShader(ShaderQueue::Opaque, L"Shaders\\public\\Opaque.hlsl", L"PS", ShaderType::Pixel);

		BindToShader();
		CreatePSO();
	}

	void OpaquePass::Dispose()
	{

	}

	void OpaquePass::Execute()
	{
		BindToShader();
	}
	void OpaquePass::Render()
	{
		Execute();

		auto cameraColorRT = GetBufferManager()->GetCameraColorRT();

		m_pCommand->AddBarrier(*cameraColorRT->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_pCommand->FlushBarrier();
		m_pCommand->ClearRenderTarget(*cameraColorRT->GetTexture(), Color(0, 0, 0));

		m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize.x, m_renderSize.y));

		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = (*m_pGraphicsPipelineStates)[ShaderQueue::Opaque].get();
		pipelineStateData.m_renderTargets = { cameraColorRT->GetTexture() };
		pipelineStateData.m_depthStencilTarget = GetBufferManager()->GetCameraDepthRT()->GetTexture();

		bool isReady = true;
		{
			if (cameraColorRT->GetTexture() == nullptr || GetBufferManager()->GetCameraDepthRT()->GetTexture() == nullptr)
			{
				ThrowRuntimeError("null texture resource");
			}
			isReady &= cameraColorRT->GetTexture()->GetIsReady();
			isReady &= GetBufferManager()->GetCameraDepthRT()->GetTexture()->GetIsReady();
		}
		if (isReady)
		{
			m_pCommand->SetPipeline(pipelineStateData);
			m_pCommand->SetPipelineResource(PER_PASS_SPACE, RenderResource::GetPerMainBindResourceSpace());

			m_pCommand->Draw(3, 0);
		}

		m_pCommand->AddBarrier(*cameraColorRT->GetTexture(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_pCommand->FlushBarrier();
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
		pipelineStateCreateDesc.m_vertexShader = GetVertexShaders()[ShaderQueue::Blit][ShaderType::Vertex].get();
		pipelineStateCreateDesc.m_pixelShader = GetPixelShaders()[ShaderQueue::Opaque][ShaderType::Pixel].get();
		pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = 1;
		pipelineStateCreateDesc.m_renderTargetDesc.m_renderTargetFormats[0] = GetBufferManager()->GetCameraColorRT()->GetFormat();
		pipelineStateCreateDesc.m_renderTargetDesc.m_depthStencilFormat = GetBufferManager()->GetCameraDepthRT()->GetFormat();
		pipelineStateCreateDesc.m_depthStencilDesc = GetDepthState(DepthState::Disabled);
		pipelineStateCreateDesc.m_blendDesc = GetBlendState(BlendState::Disabled);
		pipelineStateCreateDesc.m_rasterDesc = GetRasterizerState(RasterizerState::NoCullNoMS);
		pipelineStateCreateDesc.m_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		(*m_pGraphicsPipelineStates)[ShaderQueue::Opaque] = std::move(GetDevice()->CreateGraphicsPipelineState(pipelineStateCreateDesc, meshResourceLayout));
	}
}