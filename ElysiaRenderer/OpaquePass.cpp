#include "stdafx.h"
#include "OpaquePass.h"

#include "DX12Device.h"
#include "RenderTexture.h"


namespace ElysiaRenderer
{
	int OpaquePass::ShaderPasseIDs::OpaqueLightPassID = -1;

	OpaquePass::OpaquePass(DX12Camera* pCamera):
		BasePass(pCamera)
	{

	}

	OpaquePass::~OpaquePass()
	{
		Dispose();
	}
	void OpaquePass::Dispose()
	{

	}

	void OpaquePass::Configure()
	{
		m_shaderPasses.emplace_back(ShaderPass
			{
				.Name = "Opaque Light Pass",
				.FilePath = L"Shaders\\public\\Opaque.hlsl",
				.RasterizerDesc = GetRasterizerState(RasterizerState::NoCullNoMS),
				.BlendDesc = GetBlendState(BlendState::Disabled),
				.DepthStencilDesc = GetDepthState(DepthState::Disabled) 
			});
		m_pMaterial = std::make_unique<RenderMaterial>(m_shaderPasses);
		ShaderPasseIDs::OpaqueLightPassID = m_pMaterial->FindPassIndex("Opaque Light Pass");

		{
			RenderTargetDesc RTDesc = RenderTargetDesc
			{
				.m_renderTargetFormats = GetBufferManager()->GetCameraColorRT()->GetFormat(),
				.m_numRenderTargets = 1,
				.m_depthStencilFormat = GetBufferManager()->GetCameraDepthRT()->GetFormat()
			};
			auto emplaceResult = m_PipelineStateObjects.try_emplace(ShaderPasseIDs::OpaqueLightPassID);
			if (emplaceResult.second)
			{
				emplaceResult.first->second = GetPSOManager()->GetGraphicsPipelineState(
					m_pMaterial.get(), ShaderPasseIDs::OpaqueLightPassID, RTDesc);
			}
		}
	}

	void OpaquePass::Execute()
	{
		auto t1 = GetScreenSize(Vector2(m_renderSize.x, m_renderSize.y));
		auto t2 = m_pCamera->GetViewMat();
		auto t3 = m_pCamera->GetViewMat().Invert();
		auto t4 = m_pCamera->GetProjMat();
		auto t5 = m_pCamera->GetProjMat().Invert();
		auto t6 = m_pCamera->GetViewMat() * m_pCamera->GetProjMat();
		auto t7 = (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert();

		m_pMaterial->SetConstantVariable("screenSize", &t1);

		m_pMaterial->SetConstantVariable("viewMatrix", &t2);
		m_pMaterial->SetConstantVariable("viewMatrix_I", &t3);
		m_pMaterial->SetConstantVariable("projMatrix", &t4);
		m_pMaterial->SetConstantVariable("projMatrix_I", &t5);
		m_pMaterial->SetConstantVariable("viewProjMatrix", &t6);
		m_pMaterial->SetConstantVariable("viewProjMatrix_I", &t7);

		//m_pMaterial->ApplyConstantData();
	}
	void OpaquePass::Render()
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "Opaque Light Pass");

		Execute();

		auto cameraColorRT = GetBufferManager()->GetCameraColorRT();

		m_pCommand->AddBarrier(*cameraColorRT->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_pCommand->FlushBarrier();
		m_pCommand->ClearRenderTarget(*cameraColorRT->GetTexture(), Color(0, 0, 0, 0));

		m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize));
		m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		PipelineInfo pipelineStateData
		{
			.m_pipelineStateObject = m_PipelineStateObjects[ShaderPasseIDs::OpaqueLightPassID],
			.m_renderTargets = { cameraColorRT->GetTexture() },
			.m_depthStencilTarget = GetBufferManager()->GetCameraDepthRT()->GetTexture()
		};

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

			m_pCommand->SetPipelineResource(PER_PASS_SPACE, m_pMaterial->GetPassData(ShaderPasseIDs::OpaqueLightPassID).MeshResourceLayouts->m_spaces[PER_PASS_SPACE]);

			m_pCommand->Draw(3, 0);
		}

		m_pCommand->AddBarrier(*cameraColorRT->GetTexture(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_pCommand->FlushBarrier();
	}
}