#include "stdafx.h"
#include "TonemapPass.h"

#include "DX12Device.h"
#include "RenderTexture.h"

namespace ElysiaRenderer
{
	int TonemapPass::ShaderPasseIDs::BlitPassID = -1;
	int TonemapPass::ShaderPasseIDs::TonemapPassID = -1;

	TonemapPass::TonemapPass(DX12Camera* pCamera) : 
		BasePass(pCamera)
	{

	}
	TonemapPass::~TonemapPass()
	{
		Dispose();
	}
	void TonemapPass::Dispose()
	{

	}

	void TonemapPass::Configure()
	{
		if (!UserData::GetInstance().IsUseHDR)
		{
			m_pTempRT = CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
				static_cast<UINT64>(m_renderSize.y),
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
				L"Temp RT");
		}
		else
		{
			switch (UserData::GetInstance().HDRLevel)
			{
				case HDRQuality::Low:
				{
					m_pTempRT = CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
						static_cast<UINT64>(m_renderSize.y),
						DXGI_FORMAT_R11G11B10_FLOAT,
						L"Temp RT");
					break;
				}
				case HDRQuality::High:
				{
					m_pTempRT = CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
						static_cast<UINT64>(m_renderSize.y),
						DXGI_FORMAT_R16G16B16A16_FLOAT,
						L"Temp RT");
					break;
				}
				default:
				{
					ThrowRuntimeError("Invalid choose");
					break;
				}
			}
		}
		

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
			ShaderPass
			{
				.Name = "Tonemap Pass",
				.FilePath = L"Shaders\\public\\TonemapPass.hlsl",
				.RasterizerDesc = GetRasterizerState(RasterizerState::NoCullNoMS),
				.BlendDesc = GetBlendState(BlendState::Disabled),
				.DepthStencilDesc = GetDepthState(DepthState::Disabled)
			}
		};

		m_pMaterial = std::make_unique<RenderMaterial>(m_shaderPasses);
		ShaderPasseIDs::BlitPassID = m_pMaterial->FindPassIndex("Blit Pass");
		ShaderPasseIDs::TonemapPassID = m_pMaterial->FindPassIndex("Tonemap Pass");

		{
			RenderTargetDesc RTDesc = RenderTargetDesc
			{
				.m_renderTargetFormats = m_pTempRT->GetFormat(),
				.m_numRenderTargets = 1,
				.m_depthStencilFormat = GetBufferManager()->GetCameraDepthRT()->GetFormat()
			};

			auto emplaceResult = m_PipelineStateObjects.try_emplace(ShaderPasseIDs::BlitPassID);
			if (emplaceResult.second)
			{
				emplaceResult.first->second = GetPSOManager()->GetGraphicsPipelineState(m_pMaterial.get(), ShaderPasseIDs::BlitPassID, RTDesc);
			}
		}
		{
			RenderTargetDesc RTDesc = RenderTargetDesc
			{
				.m_renderTargetFormats = GetBufferManager()->GetCameraColorRT()->GetFormat(),
				.m_numRenderTargets = 1,
				.m_depthStencilFormat = GetBufferManager()->GetCameraDepthRT()->GetFormat()
			};
			auto emplaceResult = m_PipelineStateObjects.try_emplace(ShaderPasseIDs::TonemapPassID);
			if (emplaceResult.second)
			{
				emplaceResult.first->second = GetPSOManager()->GetGraphicsPipelineState(m_pMaterial.get(), ShaderPasseIDs::TonemapPassID, RTDesc);
			}
		}
	}
	void TonemapPass::Execute()
	{
		


	}
	void TonemapPass::Render()
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "Tonemap Pass");

		Execute();
		{
			m_pCommand->AddBarrier(*m_pTempRT->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
			m_pCommand->FlushBarrier();
			m_pCommand->ClearRenderTarget(*m_pTempRT->GetTexture(), Color(0, 0, 0, 0));

			m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize));
			m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			PipelineInfo pipelineStateData{};
			pipelineStateData.m_pipelineStateObject = m_PipelineStateObjects[ShaderPasseIDs::BlitPassID];
			pipelineStateData.m_renderTargets = { m_pTempRT->GetTexture() };
			pipelineStateData.m_depthStencilTarget = GetBufferManager()->GetCameraDepthRT()->GetTexture();

			bool isReady = true;
			{
				if (m_pTempRT->GetTexture() == nullptr || GetBufferManager()->GetCameraDepthRT()->GetTexture() == nullptr)
				{
					ThrowRuntimeError("null texture resource");
				}
				isReady &= m_pTempRT->GetTexture()->GetIsReady();
				isReady &= GetBufferManager()->GetCameraDepthRT()->GetTexture()->GetIsReady();
			}
			if (isReady)
			{
				m_pCommand->SetPipeline(pipelineStateData);
				m_pMaterial->SetConstantVariable("blitterTextureIndex", GetBufferManager()->GetCameraColorRT()->GetTexture()->GetResourceHeapIndex(), ShaderPasseIDs::BlitPassID);
				m_pMaterial->ApplyConstantData();
				m_pCommand->SetPipelineResource(PER_PASS_SPACE, m_pMaterial->GetPassData(ShaderPasseIDs::BlitPassID).MeshResourceLayouts->m_spaces[PER_PASS_SPACE]);

				m_pCommand->DrawFullScreenTriangle();
			}

			m_pCommand->AddBarrier(*m_pTempRT->GetTexture(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			m_pCommand->FlushBarrier();
		}

		{
			auto cameraColorRT = GetBufferManager()->GetCameraColorRT();

			m_pCommand->AddBarrier(*cameraColorRT->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
			m_pCommand->FlushBarrier();
			m_pCommand->ClearRenderTarget(*cameraColorRT->GetTexture(), Color(0, 0, 0, 0));

			m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize));
			m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			PipelineInfo pipelineStateData{};
			pipelineStateData.m_pipelineStateObject = m_PipelineStateObjects[ShaderPasseIDs::TonemapPassID];
			pipelineStateData.m_renderTargets = { cameraColorRT->GetTexture() };
			pipelineStateData.m_depthStencilTarget = GetBufferManager()->GetCameraDepthRT()->GetTexture();

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
				m_pMaterial->SetConstantVariable("blitterTextureIndex", m_pTempRT->GetTexture()->GetResourceHeapIndex(), ShaderPasseIDs::TonemapPassID);
				m_pMaterial->ApplyConstantData();
				m_pCommand->SetPipelineResource(PER_PASS_SPACE, m_pMaterial->GetPassData(ShaderPasseIDs::TonemapPassID).MeshResourceLayouts->m_spaces[PER_PASS_SPACE]);

				m_pCommand->DrawFullScreenTriangle();
			}

			m_pCommand->AddBarrier(*cameraColorRT->GetTexture(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			m_pCommand->FlushBarrier();
		}
	}
}