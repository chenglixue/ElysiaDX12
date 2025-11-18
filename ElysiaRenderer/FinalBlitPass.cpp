#include "stdafx.h"
#include "FinalBlitPass.h"

#include "DX12Device.h"
#include "RenderTexture.h"

namespace ElysiaRenderer
{
	int FinalBlitPass::ShaderPassIDs::BlitPassID = -1;

	FinalBlitPass::FinalBlitPass(DX12Camera* pCamera) :
		BasePass(pCamera)
	{

	}

	FinalBlitPass::~FinalBlitPass()
	{
		Dispose();
	}
	void FinalBlitPass::Dispose()
	{

	}

	void FinalBlitPass::Configure()
	{
		m_shaderPasses = std::vector<ShaderPass>
		{
			ShaderPass
			{
				.Name = "Blit Pass",
				.FilePath = L"Shaders\\public\\Blit.hlsl",
				.VertexEntryPoint = L"BlitVS",
				.FragmentEntryPoint = L"BlitPS",
				.RasterizerDesc = GetRasterizerState(RasterizerState::NoCullNoMS),
				.BlendDesc = GetBlendState(BlendState::Disabled),
				.DepthStencilDesc = GetDepthState(DepthState::Disabled)
			},
		};

		m_pMaterial = std::make_unique<RenderMaterial>(m_shaderPasses);
		ShaderPassIDs::BlitPassID = m_pMaterial->FindPassIndex("Blit Pass");

		RenderTargetDesc RTDesc = RenderTargetDesc
		{
			.m_renderTargetFormats = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			.m_numRenderTargets = 1,
			.m_depthStencilFormat = GetBufferManager()->GetCameraDepthRT()->GetFormat()
		};
		auto emplaceResult = m_PipelineStateObjects.try_emplace(ShaderPassIDs::BlitPassID);
		if (emplaceResult.second)
		{
			emplaceResult.first->second = GetPSOManager()->GetGraphicsPipelineState(m_pMaterial.get(), ShaderPassIDs::BlitPassID, RTDesc);
		}
	}
	void FinalBlitPass::Execute()
	{
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
		pipelineStateData.m_pipelineStateObject = m_PipelineStateObjects[ShaderPassIDs::BlitPassID];
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
			m_pMaterial->SetConstantVariable("blitterTextureIndex", GetBufferManager()->GetCameraColorRT()->GetTexture()->GetResourceHeapIndex());
			m_pMaterial->ApplyConstantData();
			m_pCommand->SetPipeline(pipelineStateData);
			m_pCommand->SetPipelineResource(PER_PASS_SPACE, m_pMaterial->GetPassData(ShaderPassIDs::BlitPassID).MeshResourceLayouts->m_spaces[PER_PASS_SPACE]);

			m_pCommand->Draw(3, 0);
		}

		m_pCommand->AddBarrier(GetDevice()->GetCurrBackBuffer(), D3D12_RESOURCE_STATE_PRESENT);
		m_pCommand->FlushBarrier();
	}
}