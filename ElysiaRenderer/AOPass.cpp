#include "stdafx.h"
#include "AOPass.h"

#include "DX12Device.h"
#include "RenderTexture.h"

namespace ElysiaRenderer
{
	int AOPass::ShaderPasseIDs::AOPassID = -1;

	AOPass::AOPass(DX12Camera* pCamera) :
		BasePass(pCamera)
	{

	}
	AOPass::~AOPass()
	{
		Dispose();
	}
	void AOPass::Dispose()
	{

	}

	void AOPass::Configure()
	{
		m_pAORT = CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
			static_cast<UINT64>(m_renderSize.y),
			DXGI_FORMAT_R16_UNORM,
			L"AO RT");

		m_shaderPasses = std::vector<ShaderPass>
		{
			ShaderPass
			{
				.Name = "AO Pass",
				.FilePath = L"Shaders\\public\\SSAO.hlsl",
				.VertexEntryPoint = L"BlitVS",
				.FragmentEntryPoint = L"SSAOPS",
				.RasterizerDesc = GetRasterizerState(RasterizerState::NoCullNoMS),
				.BlendDesc = GetBlendState(BlendState::Disabled),
				.DepthStencilDesc = GetDepthState(DepthState::Disabled)
			}
		};

		m_pMaterial = std::make_unique<RenderMaterial>(m_shaderPasses);
		ShaderPasseIDs::AOPassID = m_pMaterial->FindPassIndex("AO Pass");

		{
			RenderTargetDesc RTDesc = RenderTargetDesc
			{
				.m_renderTargetFormats = m_pAORT->GetFormat(),
				.m_numRenderTargets = 1,
				.m_depthStencilFormat = GetBufferManager()->GetCameraDepthRT()->GetFormat()
			};

			auto emplaceResult = m_PipelineStateObjects.try_emplace(ShaderPasseIDs::AOPassID);
			if (emplaceResult.second)
			{
				emplaceResult.first->second = GetPSOManager()->GetGraphicsPipelineState(m_pMaterial.get(), ShaderPasseIDs::AOPassID, RTDesc);
			}
		}
	}

	void AOPass::Execute()
	{
		m_pMaterial->SetConstantVariable("g_ScreenSize", GetScreenSize(Vector2(m_renderSize.x, m_renderSize.y)));
		m_pMaterial->SetConstantVariable("viewMatrix", m_pCamera->GetViewMat());
		m_pMaterial->SetConstantVariable("viewMatrix_I", m_pCamera->GetViewMat().Invert());
		m_pMaterial->SetConstantVariable("projMatrix", m_pCamera->GetProjMat());
		m_pMaterial->SetConstantVariable("projMatrix_I", m_pCamera->GetProjMat().Invert());
		m_pMaterial->SetConstantVariable("viewProjMatrix", m_pCamera->GetViewMat() * m_pCamera->GetProjMat());
		m_pMaterial->SetConstantVariable("viewProjMatrix_I", (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert());

		m_pMaterial->SetConstantVariable("g_AOSampleCount", );
		m_pMaterial->SetConstantVariable("g_AORadius", );
		m_pMaterial->SetConstantVariable("g_AOThreshold", );
		m_pMaterial->SetConstantVariable("g_AODepthBias", );
		m_pMaterial->SetConstantVariable("g_AOSampleKernelArray", );


	}

	void AOPass::Render()
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "AO Pass");

		Execute();

		DoCalcAO();
	}

	void AOPass::DoCalcAO()
	{
		m_pCommand->AddBarrier(*m_pAORT->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_pCommand->FlushBarrier();
		m_pCommand->ClearRenderTarget(*m_pAORT->GetTexture(), Color::White);

		m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize));
		m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = m_PipelineStateObjects[ShaderPasseIDs::AOPassID];
		pipelineStateData.m_renderTargets = { m_pAORT->GetTexture() };
		pipelineStateData.m_depthStencilTarget = GetBufferManager()->GetCameraDepthRT()->GetTexture();

		bool isReady = true;
		{
			if (m_pAORT->GetTexture() == nullptr || GetBufferManager()->GetCameraDepthRT()->GetTexture() == nullptr)
			{
				ThrowRuntimeError("null texture resource");
			}
			isReady &= m_pAORT->GetTexture()->GetIsReady();
			isReady &= GetBufferManager()->GetCameraDepthRT()->GetTexture()->GetIsReady();
		}
		if (isReady)
		{
			m_pCommand->SetPipeline(pipelineStateData);
			m_pMaterial->ApplyConstantData();
			m_pCommand->SetPipelineResource(PER_PASS_SPACE, m_pMaterial->GetPassData(ShaderPasseIDs::AOPassID).MeshResourceLayouts->m_spaces[PER_PASS_SPACE]);

			m_pCommand->Draw(3, 0);
		}

		m_pCommand->AddBarrier(*m_pAORT->GetTexture(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_pCommand->FlushBarrier();
	}
}