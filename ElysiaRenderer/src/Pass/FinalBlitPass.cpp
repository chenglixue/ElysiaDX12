#include "stdafx.h"
#include "FinalBlitPass.h"

#include "lib/DX12/DX12Device.h"
#include "lib/Utility/RenderTexture.h"

namespace ElysiaRenderer
{
	int FinalBlitPass::ShaderPassIDs::BlitPassID = -1;
	size_t FinalBlitPass::ShaderIDs::blitterTextureIndex = SIZE_MAX;

	FinalBlitPass::FinalBlitPass(DX12Camera* pCamera) :
		BasePass(pCamera)
	{
		ShaderIDs::blitterTextureIndex = PropertyToID("blitterTextureIndex");
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
		m_shaderPasses = 
		{
			ShaderPass
			{
				.Name = "Blit Pass",
				.FilePath = L"Shaders\\public\\Blit.hlsl",
			},
		};
		m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
		ShaderPassIDs::BlitPassID = m_pMaterial->FindPassIndex("Blit Pass");

		UpdateVariant();
	}
	void FinalBlitPass::Execute()
	{
		UpdatePSO();
	}
	void FinalBlitPass::Render()
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "Final Blit Pass");

		Execute();
		DoFinalBlit();
	}

	void FinalBlitPass::DoFinalBlit()
	{
		auto& backBuffer = m_pDevice->GetCurrBackBuffer();

		m_pCommand->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_pCommand->ClearRenderTarget(backBuffer, Color(0, 0, 0, 0));

		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(ShaderPassIDs::BlitPassID).pPipelineStateObject;
		pipelineStateData.m_renderTargets.emplace_back(&backBuffer);
		pipelineStateData.m_depthStencilTarget = BufferManager::GetInstance().GetCameraDepthRT()->GetTexture();

		bool isReady = true;
		{
			if (BufferManager::GetInstance().GetCameraDepthRT() == nullptr)
			{
				ThrowRuntimeError("nullptr");
			} 
			isReady &= BufferManager::GetInstance().GetCameraDepthRT()->GetTexture()->GetIsReady();
		}
		if (isReady) 
		{ 
			switch (UserData::GetInstance().debugMode)
			{
			case DebugMode::None:
				{
					m_pMaterial->SetUInt(ShaderIDs::blitterTextureIndex, BufferManager::GetInstance().GetCameraColorRT()->GetTexture()->GetResourceHeapIndex());

					break;
				}
			case DebugMode::AO:
				{
					m_pMaterial->SetUInt(ShaderIDs::blitterTextureIndex, TextureManager::GetInstance().GetGlobalRT("g_AOIndex"));


					break;
				}
			}
			
			m_pCommand->SetDefaultViewportAndScissor(UINT2(m_renderSize));
			m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			m_pCommand->SetPipeline(pipelineStateData);
			
			auto& passData = m_pMaterial->GetPassData(ShaderPassIDs::BlitPassID);
			SetSpaceResource(passData, PER_PASS_SPACE);

			m_pCommand->DrawFullScreenTriangle();
		}
		
		if (ImGui::GetDrawData())
		{
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_pCommand->GetCommandList());
		}

		m_pCommand->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT);
	}

	void FinalBlitPass::UpdatePSO()
	{
		if(m_backBufferFormat != m_pDevice->GetSwapChainFormat())
		{
			{
				RenderTargetDesc RTDesc = RenderTargetDesc
				{
					.m_renderTargetFormats = m_pDevice->GetSwapChainFormat(),
					.m_numRenderTargets = 1,
					.m_depthStencilFormat = BufferManager::GetInstance().GetCameraDepthRT()->GetFormat()
				};
				m_backBufferFormat = m_pDevice->GetSwapChainFormat();

				m_PipelineStateObjects[ShaderPassIDs::BlitPassID] = PSOManager::GetInstance().GetGraphicsPipelineState(m_pDevice,
					m_pMaterial.get(), ShaderPassIDs::BlitPassID, RTDesc);
			} 
		}
	}

	void FinalBlitPass::UpdateVariant()
	{
		UpdateFinalBlitVariant(ShaderPassIDs::BlitPassID);
	}
	void FinalBlitPass::UpdateFinalBlitVariant(UINT passID)
	{
		std::vector<std::wstring> enableKeywords{};
		 
		auto& passData = m_pMaterial->GetPassData(passID);
		
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
					.m_renderTargetFormats = m_pDevice->GetSwapChainFormat(),
					.m_numRenderTargets = 1,
					.m_depthStencilFormat = BufferManager::GetInstance().GetCameraDepthRT()->GetFormat()
				};
				passData.pPipelineStateObject = PSOManager::GetInstance().GetGraphicsPipelineState(m_pDevice, m_pMaterial.get(), passID, RTDesc);

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