#include "stdafx.h"
#include "OpaquePass.h"

#include "lib/DX12/DX12Device.h"
#include "lib/Utility/RenderTexture.h"
#include "RenderResource.h"

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
			m_cameraColorFormat = GetBufferManager()->GetCameraColorRT()->GetFormat();
			auto emplaceResult = m_PipelineStateObjects.try_emplace(ShaderPasseIDs::OpaqueLightPassID);
			if (emplaceResult.second)
			{
				emplaceResult.first->second = GetPSOManager()->GetGraphicsPipelineState(
					m_pMaterial.get(), ShaderPasseIDs::OpaqueLightPassID, RTDesc);
			}
		}

		m_pMaterial->SetConstantVariable("g_AOIndex", TextureManager::GetInstance().GetGlobalRT("g_AOIndex"));
	}

	void OpaquePass::Execute()
	{
		UpdatePSO();
		
		m_pMaterial->SetConstantVariable("screenSize", GetScreenSize(Vector2(m_renderSize.x, m_renderSize.y)));
		m_pMaterial->SetConstantVariable("viewMatrix", m_pCamera->GetViewMat());
		m_pMaterial->SetConstantVariable("viewMatrix_I", m_pCamera->GetViewMat().Invert());
		m_pMaterial->SetConstantVariable("projMatrix", m_pCamera->GetProjMat());
		m_pMaterial->SetConstantVariable("projMatrix_I", m_pCamera->GetProjMat().Invert());
		m_pMaterial->SetConstantVariable("viewProjMatrix", m_pCamera->GetViewMat() * m_pCamera->GetProjMat());
		m_pMaterial->SetConstantVariable("viewProjMatrix_I", (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert());

		m_pMaterial->ApplyConstantData();
	}
	void OpaquePass::Render()
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "Opaque Light Pass");

		Execute();

		auto cameraColorRT = GetBufferManager()->GetCameraColorRT();

		m_pCommand->AddBarrier(cameraColorRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_pCommand->ClearRenderTarget(cameraColorRT, Color::Black);

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
			m_pCommand->SetPipelineResource(PER_FRAME_SPACE, GetRenderResource()->GetPerFrameBindResourceSpace());

			m_pCommand->DrawFullScreenTriangle();
		}

		m_pCommand->AddBarrier(cameraColorRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	void OpaquePass::UpdatePSO()
	{
		if(m_cameraColorFormat != GetBufferManager()->GetCameraColorRT()->GetFormat())
		{
			{
				RenderTargetDesc RTDesc = RenderTargetDesc
				{
					.m_renderTargetFormats = GetBufferManager()->GetCameraColorRT()->GetFormat(),
					.m_numRenderTargets = 1,
					.m_depthStencilFormat = GetBufferManager()->GetCameraDepthRT()->GetFormat()
				};
				m_cameraColorFormat = GetBufferManager()->GetCameraColorRT()->GetFormat();

				auto emplaceResult = m_PipelineStateObjects.try_emplace(ShaderPasseIDs::OpaqueLightPassID);
				emplaceResult.first->second = GetPSOManager()->GetGraphicsPipelineState(m_pMaterial.get(), ShaderPasseIDs::OpaqueLightPassID, RTDesc);
			} 
		}
	}
}