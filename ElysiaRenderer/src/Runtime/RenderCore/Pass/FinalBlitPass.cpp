#include "stdafx.h"
#include "FinalBlitPass.h"

#include "Editor/UserData.h"
#include "Programs/PIXHelper.h"

#include "Runtime/Core/DX12GraphicsContext.h"
#include "Runtime/Core/DX12Shader.h"
#include "Runtime/Core/DX12Device.h"
#include "Runtime/Core/DX12TextureBuffer.h"
#include "Runtime/Core/SwapChain.h"

#include "Runtime/RenderCore/RenderResource.h"
#include "Runtime/RenderCore/RenderTexture.h"
#include "Runtime/RenderCore/Material.h"
#include "Runtime/RenderCore/PSOManager.h"
#include "Runtime/RenderCore/RenderTargetManager.h"
#include "Runtime/RenderCore/ShaderVariantManager.h"

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
	void FinalBlitPass::Render(ElysiaEngine::FrameContext& context)
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "Final Blit Pass");

		UpdatePSO();
		
		DoFinalBlit();
	}

	void FinalBlitPass::DoFinalBlit()
	{
		auto& backBuffer = m_pSwaiChain->GetCurrBackBuffer();

		m_pCommand->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_pCommand->ClearRenderTarget(backBuffer, Color::Black);
		
		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(ShaderPassIDs::BlitPassID).pPipelineStateObject;
		pipelineStateData.m_renderTargets.emplace_back(&backBuffer);
		pipelineStateData.m_depthStencilTarget = m_pCameraDepthRT->GetTexture();
			m_pCommand->SetPipeline(pipelineStateData);
		
		auto& passData = m_pMaterial->GetPassData(ShaderPassIDs::BlitPassID);
		SetSpaceResource(passData, PER_PASS_SPACE);

		bool isReady = true;
		{
			if (m_pCameraDepthRT == nullptr)
			{
				ThrowRuntimeError("nullptr");
			} 
			isReady &= m_pCameraDepthRT->GetTexture()->GetIsReady();
		}
		if (isReady) 
		{ 
			switch (UserData::GetInstance().debugMode)
			{
			case DebugMode::None:
				{
					m_pMaterial->SetUInt(ShaderIDs::blitterTextureIndex, m_pCameraColorRT->GetResourceHeapIndex());

					break;
				}
			case DebugMode::AO:
				{
					//m_pMaterial->SetUInt(ShaderIDs::blitterTextureIndex, TextureManager::GetInstance().GetGlobalRT("g_AOIndex"));
					break;
				}
				default:
				{
					m_pMaterial->SetUInt(ShaderIDs::blitterTextureIndex, m_pCameraColorRT->GetResourceHeapIndex());

					break;
				}
			}
			
			m_pCommand->SetDefaultViewportAndScissor(UINT2(m_renderSize));
			m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			

			m_pCommand->DrawFullScreenTriangle();
		}
		
		

		m_pCommand->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT);
	}

	void FinalBlitPass::UpdatePSO()
	{
		if(m_backBufferFormat != m_pSwaiChain->GetFormat())
		{
			{
				RenderTargetDesc RTDesc = RenderTargetDesc
				{
					.m_renderTargetFormats = m_pSwaiChain->GetFormat(),
					.m_numRenderTargets = 1,
					.m_depthStencilFormat = m_pCameraDepthRT->GetFormat()
				};
				m_backBufferFormat = m_pSwaiChain->GetFormat();

				m_pMaterial->GetPassData(ShaderPassIDs::BlitPassID).pPipelineStateObject = PSOManager::GetInstance().GetGraphicsPipelineState(m_pDevice,
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
					.m_renderTargetFormats = m_pSwaiChain->GetFormat(),
					.m_numRenderTargets = 1,
					.m_depthStencilFormat = m_pCameraDepthRT->GetFormat()
				};
				m_backBufferFormat = m_pSwaiChain->GetFormat();
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