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

		BindToShader();
		CreatePSO();
	}
	void FinalBlitPass::Execute()
	{
		BindToShader();
	}
	void FinalBlitPass::Render()
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "Final Blit Pass");

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
		GetBufferManager()->GetSingleConstantBuffer(PER_PASS_SPACE)->SetMappedData(RenderResource::GetInstance().GetCBVPassParameter(), sizeof(CBVMainPassParameter));
	}
	void FinalBlitPass::CreatePSO()
	{

	}
}