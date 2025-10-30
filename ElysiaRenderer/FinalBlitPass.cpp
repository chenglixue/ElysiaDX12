#include "stdafx.h"
#include "FinalBlitPass.h"

#include "DX12Device.h"
#include "RenderTexture.h"

namespace ElysiaRenderer
{
	FinalBlitPass::~FinalBlitPass()
	{
		Dispose();
	}

	void FinalBlitPass::Dispose()
	{

	}

	void FinalBlitPass::Configure()
	{
		AddShader(ShaderQueue::Blit, L"Shaders\\public\\Blit.hlsl", L"PS", ShaderType::Pixel);

		BindToShader();
		CreatePSO();
	}
	void FinalBlitPass::Execute()
	{
		BindToShader();
	}
	void FinalBlitPass::Render()
	{
		Execute();
		auto& cameraColorRT = GetDevice()->GetCurrBackBuffer();

		m_pCommand->AddBarrier(cameraColorRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_pCommand->FlushBarrier();
		m_pCommand->ClearRenderTarget(cameraColorRT, Color(0, 0, 0, 0));

		m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(static_cast<UINT>(m_renderSize.x), static_cast<UINT>(m_renderSize.y)));

		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = (*m_pGraphicsPipelineStates)[ShaderQueue::Blit].get();
		pipelineStateData.m_renderTargets.emplace_back(&cameraColorRT);
		pipelineStateData.m_depthStencilTarget = GetBufferManager()->GetCameraDepthRT()->GetTexture();

		bool isReady = true;
		{
			if (GetBufferManager()->GetCameraDepthRT() == nullptr)
			{
				ThrowRuntimeError("nullptr");;
			}
			isReady &= GetBufferManager()->GetCameraDepthRT()->GetTexture()->GetIsReady();
		}
		if (isReady)
		{
			m_pCommand->SetPipeline(pipelineStateData);
			m_pCommand->SetPipelineResource(PER_PASS_SPACE, RenderResource::GetPerMainBindResourceSpace());

			m_pCommand->Draw(3, 0);
		}

		m_pCommand->AddBarrier(GetDevice()->GetCurrBackBuffer(), D3D12_RESOURCE_STATE_PRESENT);
		m_pCommand->FlushBarrier();
	}

	void FinalBlitPass::BindToShader()
	{
		RenderResource::GetInstance().GetCBVPassParameter()->blitterTextureIndex = GetBufferManager()->GetCameraColorRT()->GetTexture()->GetResourceHeapIndex();
	}
	void FinalBlitPass::CreatePSO()
	{
		PipelineResourceLayout meshResourceLayout{};
		PipelineStateCreateDesc pipelineStateCreateDesc{};

		meshResourceLayout.m_spaces[PER_OBJECT_SPACE] = RenderResource::GetPerObjectBindResourceSpace();
		meshResourceLayout.m_spaces[PER_PASS_SPACE] = RenderResource::GetPerMainBindResourceSpace();

		pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
		pipelineStateCreateDesc.m_vertexShader = GetVertexShaders()[ShaderQueue::Blit][ShaderType::Vertex].get();
		pipelineStateCreateDesc.m_pixelShader = GetPixelShaders()[ShaderQueue::Blit][ShaderType::Pixel].get();
		pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = 1;
		pipelineStateCreateDesc.m_renderTargetDesc.m_renderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		pipelineStateCreateDesc.m_renderTargetDesc.m_depthStencilFormat = GetBufferManager()->GetCameraDepthRT()->GetFormat();
		pipelineStateCreateDesc.m_depthStencilDesc = GetDepthState(DepthState::Disabled);
		pipelineStateCreateDesc.m_blendDesc = GetBlendState(BlendState::Disabled);
		pipelineStateCreateDesc.m_rasterDesc = GetRasterizerState(RasterizerState::NoCullNoMS);
		pipelineStateCreateDesc.m_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		(*m_pGraphicsPipelineStates)[ShaderQueue::Blit] = std::move(GetDevice()->CreateGraphicsPipelineState(pipelineStateCreateDesc, meshResourceLayout));

	}
}