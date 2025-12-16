#include "stdafx.h"
#include "OpaquePass.h"

#include "lib/DX12/DX12Device.h"
#include "lib/Utility/RenderTexture.h"
#include "RenderResource.h"
#include "Manager/RenderTargetManager.h"

namespace ElysiaRenderer
{
	int OpaquePass::ShaderPassIDs::OpaqueLightPassID = -1;

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
		ShaderPassIDs::OpaqueLightPassID = m_pMaterial->FindPassIndex("Opaque Light Pass");

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
		bool isReady = true;
		{
			if (m_pCameraColorRT->GetTexture() == nullptr || m_pCameraDepthRT->GetTexture() == nullptr)
			{
				ThrowRuntimeError("null texture resource");
			}
			isReady &= m_pCameraColorRT->GetTexture()->GetIsReady();
			isReady &= m_pCameraDepthRT->GetTexture()->GetIsReady();
		}
		if (isReady)
		{
			auto& passData = m_pMaterial->GetPassData(ShaderPassIDs::OpaqueLightPassID);
			assert(passData.pPipelineStateObject);
			
			m_pCommand->AddBarrier(m_pCameraColorRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
			m_pCommand->ClearRenderTarget(m_pCameraColorRT, Color::Black);
			
			PipelineInfo pipelineStateData
			{
				.m_pipelineStateObject = passData.pPipelineStateObject,
				.m_renderTargets = { m_pCameraColorRT->GetTexture() },
				.m_depthStencilTarget = m_pCameraDepthRT->GetTexture()
			};
			m_pCommand->SetPipeline(pipelineStateData);
			m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize));
			m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			
			SetSpaceResource(passData, PER_PASS_SPACE);
			SetSpaceResource(passData, PER_FRAME_SPACE);
			m_pCommand->DrawFullScreenTriangle();
			
			m_pCommand->AddBarrier(m_pCameraColorRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
		else
		{
			m_pCommand->AddBarrier(m_pCameraColorRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
			m_pCommand->ClearRenderTarget(m_pCameraColorRT, Color::Black);
			
			m_pCommand->AddBarrier(m_pCameraColorRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
	}

	void OpaquePass::UpdatePSO()
	{
		if(m_cameraColorFormat != m_pCameraColorRT->GetFormat())
		{
			{
				RenderTargetDesc RTDesc = RenderTargetDesc
				{
					.m_renderTargetFormats = m_pCameraColorRT->GetFormat(),
					.m_numRenderTargets = 1,
					.m_depthStencilFormat = m_pCameraDepthRT->GetFormat()
				};
				m_cameraColorFormat = m_pCameraColorRT->GetFormat();
				
				m_pMaterial->GetPassData(ShaderPassIDs::OpaqueLightPassID).pPipelineStateObject = PSOManager::GetInstance().GetGraphicsPipelineState(m_pDevice,
					m_pMaterial.get(), ShaderPassIDs::OpaqueLightPassID, RTDesc);
			} 
		}
	}
	void OpaquePass::UpdateVariant()
	{
		UpdateLightingPassVariant(ShaderPassIDs::OpaqueLightPassID);
	}
	void OpaquePass::UpdateLightingPassVariant(UINT passIndex)
	{
		std::vector<std::wstring> enableKeywords{};
		
		switch (UserData::GetInstance().shadowQuality)
		{
			case ShadowQuality::Low:
				{
					enableKeywords.emplace_back(L"SHADOW_QUALITY_LOW");
					break;
				}
			case ShadowQuality::Middle:
				{
					enableKeywords.emplace_back(L"SHADOW_QUALITY_MIDDLE");
					break;
				}
			case ShadowQuality::High:
				{
					enableKeywords.emplace_back(L"SHADOW_QUALITY_HIGH");
					break;
				}
			case ShadowQuality::VeryHigh:
				{
					enableKeywords.emplace_back(L"SHADOW_QUALITY_VERYHIGH");
					break;
				}
		}
		switch (UserData::GetInstance().shadowType)
		{
			case ShadowType::Hard:
				{
					enableKeywords.emplace_back(L"HARD_SHADOW");
					break;
				}
			case ShadowType::Soft:
				{
					enableKeywords.emplace_back(L"SOFT_SHADOW");
					break;
				} 
		} 
		 
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
					.m_renderTargetFormats = m_pCameraColorRT->GetFormat(),
					.m_numRenderTargets = 1,
					.m_depthStencilFormat = m_pCameraDepthRT->GetFormat()
				};
				m_cameraColorFormat = m_pCameraColorRT->GetFormat();
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
