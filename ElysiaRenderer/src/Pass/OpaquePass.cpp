#include "stdafx.h"
#include "OpaquePass.h"

#include "lib/DX12/DX12Device.h"
#include "lib/Utility/RenderTexture.h"
#include "RenderResource.h"
#include "Manager/RenderTargetManager.h"

namespace ElysiaRenderer
{
	int OpaquePass::ShaderPasseIDs::OpaqueLightPassID = -1;

	size_t OpaquePass::ShaderIDs::g_AOIndex = SIZE_MAX;
	size_t OpaquePass::ShaderIDs::screenSize = SIZE_MAX;
	size_t OpaquePass::ShaderIDs::viewMatrix = SIZE_MAX;
	size_t OpaquePass::ShaderIDs::viewMatrix_I = SIZE_MAX;
	size_t OpaquePass::ShaderIDs::projMatrix = SIZE_MAX;
	size_t OpaquePass::ShaderIDs::projMatrix_I = SIZE_MAX;
	size_t OpaquePass::ShaderIDs::viewProjMatrix = SIZE_MAX;
	size_t OpaquePass::ShaderIDs::viewProjMatrix_I = SIZE_MAX;
 
	OpaquePass::OpaquePass(DX12Camera* pCamera):
		BasePass(pCamera)
	{
		ShaderIDs::g_AOIndex = PropertyToID("g_AOIndex");

		ShaderIDs::screenSize = PropertyToID("screenSize");
		ShaderIDs::viewMatrix = PropertyToID("viewMatrix");
		ShaderIDs::viewMatrix_I = PropertyToID("viewMatrix_I");
		ShaderIDs::projMatrix = PropertyToID("projMatrix");
		ShaderIDs::projMatrix_I = PropertyToID("projMatrix_I");
		ShaderIDs::viewProjMatrix = PropertyToID("viewProjMatrix");
		ShaderIDs::viewProjMatrix_I = PropertyToID("viewProjMatrix_I");
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
		m_shaderPasses =
		{
			ShaderPass
			{
				.Name = "Opaque Light Pass",
				.FilePath = L"Shaders\\public\\Opaque.hlsl",
			}
		};
		m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
		ShaderPasseIDs::OpaqueLightPassID = m_pMaterial->FindPassIndex("Opaque Light Pass");

		UpdateVariant();

		// m_pMaterial->SetUInt(ShaderIDs::g_AOIndex,
		// 	RenderTargetManager::GetInstance().GetRenderTexture("g_AOIndex")->GetResourceHeapIndex());
	}

	void OpaquePass::Execute()
	{
		UpdatePSO();
		
		m_pMaterial->SetFloat4(ShaderIDs::screenSize, GetScreenSize(Vector2(m_renderSize.x, m_renderSize.y)));
		m_pMaterial->SetMatrix(ShaderIDs::viewMatrix, m_pCamera->GetViewMat());
		m_pMaterial->SetMatrix(ShaderIDs::viewMatrix_I, m_pCamera->GetViewMat().Invert());
		m_pMaterial->SetMatrix(ShaderIDs::projMatrix, m_pCamera->GetProjMat());
		m_pMaterial->SetMatrix(ShaderIDs::projMatrix_I, m_pCamera->GetProjMat().Invert());
		m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix, m_pCamera->GetViewMat() * m_pCamera->GetProjMat());
		m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix_I, (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert());
	}
	void OpaquePass::Render()
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "Opaque Light Pass");

		Execute();

		DrawLightingPass();
	}
	void OpaquePass::DrawLightingPass()
	{
		auto cameraColorRT = BufferManager::GetInstance().GetCameraColorRT();

		m_pCommand->AddBarrier(cameraColorRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_pCommand->ClearRenderTarget(cameraColorRT, Color::Black);

		m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize));
		m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		PipelineInfo pipelineStateData
		{
			.m_pipelineStateObject = m_PipelineStateObjects[ShaderPasseIDs::OpaqueLightPassID],
			.m_renderTargets = { cameraColorRT->GetTexture() },
			.m_depthStencilTarget = BufferManager::GetInstance().GetCameraDepthRT()->GetTexture()
		};

		bool isReady = true;
		{
			if (cameraColorRT->GetTexture() == nullptr || BufferManager::GetInstance().GetCameraDepthRT()->GetTexture() == nullptr)
			{
				ThrowRuntimeError("null texture resource");
			}
			isReady &= cameraColorRT->GetTexture()->GetIsReady();
			isReady &= BufferManager::GetInstance().GetCameraDepthRT()->GetTexture()->GetIsReady();
		}
		if (isReady)
		{
			m_pCommand->SetPipeline(pipelineStateData);

			auto& passData = m_pMaterial->GetPassData(ShaderPasseIDs::OpaqueLightPassID);
			SetSpaceResource(passData, PER_PASS_SPACE);
			SetSpaceResource(passData, PER_FRAME_SPACE);

			m_pCommand->DrawFullScreenTriangle();
		}
		
		m_pCommand->AddBarrier(cameraColorRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	void OpaquePass::UpdatePSO()
	{
		if(m_cameraColorFormat != BufferManager::GetInstance().GetCameraColorRT()->GetFormat())
		{
			{
				RenderTargetDesc RTDesc = RenderTargetDesc
				{
					.m_renderTargetFormats = BufferManager::GetInstance().GetCameraColorRT()->GetFormat(),
					.m_numRenderTargets = 1,
					.m_depthStencilFormat = BufferManager::GetInstance().GetCameraDepthRT()->GetFormat()
				};
				m_cameraColorFormat = BufferManager::GetInstance().GetCameraColorRT()->GetFormat();

				auto emplaceResult = m_PipelineStateObjects.try_emplace(ShaderPasseIDs::OpaqueLightPassID);
				emplaceResult.first->second = PSOManager::GetInstance().GetGraphicsPipelineState(m_pDevice,
					m_pMaterial.get(), ShaderPasseIDs::OpaqueLightPassID, RTDesc);
			} 
		}
	}
	void OpaquePass::UpdateVariant()
	{
		UpdateLightingPassVariant(ShaderPasseIDs::OpaqueLightPassID);
	}
	void OpaquePass::UpdateLightingPassVariant(UINT passIndex)
	{
		std::vector<std::wstring> enableKeywords{};
		 
		auto& passData = m_pMaterial->GetPassData(passIndex);
		
		auto emplaceResult = passData.keywords.try_emplace(enableKeywords);
		if(emplaceResult.second)
		{
			auto VariantManager = passData.pShader->GetVariantManager();
			auto currVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);
			
			if(passData.pCurrVariantData == nullptr || passData.pCurrVariantData != currVariantData)
			{
				passData.pCurrVariantData = currVariantData;
			}
			
			{
				RenderTargetDesc RTDesc = RenderTargetDesc
				{
					.m_renderTargetFormats = BufferManager::GetInstance().GetCameraColorRT()->GetFormat(),
					.m_numRenderTargets = 1,
					.m_depthStencilFormat = BufferManager::GetInstance().GetCameraDepthRT()->GetFormat()
				};
				passData.pPipelineStateObject = PSOManager::GetInstance().GetGraphicsPipelineState(m_pDevice, m_pMaterial.get(), passIndex, RTDesc);

				emplaceResult.first->second = 
				{
					.pCurrVariantData = currVariantData,
					.pPipelineStateObject = passData.pPipelineStateObject,
				};
			}
		}
		else
		{
			const auto& saveData = passData.keywords.at(enableKeywords);
			
			passData.pCurrVariantData = saveData.pCurrVariantData;
			passData.pPipelineStateObject = saveData.pPipelineStateObject;
		}
	}
}
