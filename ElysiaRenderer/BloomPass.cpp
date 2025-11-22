#include "stdafx.h"
#include "BloomPass.h"

#include "DX12Device.h"
#include "RenderTexture.h"
#include "RenderResource.h"

namespace ElysiaRenderer
{
	int BloomPass::ShaderPasseIDs::BloomPassID = -1;
	int BloomPass::ShaderPasseIDs::BlitPassID = -1;

	BloomPass::BloomPass(DX12Camera* pCamera) :
		BasePass(pCamera)
	{

	}
	BloomPass::~BloomPass()
	{
		Dispose();
	}
	void BloomPass::Dispose()
	{

	}

	void BloomPass::Configure()
	{
		if (!UserData::GetInstance().IsUseHDR)
		{
			m_pBloomRT = CreateRWRenderTexture(static_cast<UINT64>(m_renderSize.x),
				static_cast<UINT64>(m_renderSize.y),
				DXGI_FORMAT_R8G8B8A8_UNORM,
				true,
				L"Bloom RT");
		}
		else
		{
			switch (UserData::GetInstance().HDRLevel)
			{
				case HDRQuality::Low:
				{
					m_pBloomRT = CreateRWRenderTexture(static_cast<UINT64>(m_renderSize.x),
						static_cast<UINT64>(m_renderSize.y),
						DXGI_FORMAT_R11G11B10_FLOAT,
						true,
						L"Temp RT");
					break;
				}
				case HDRQuality::High:
				{
					m_pBloomRT = CreateRWRenderTexture
						(static_cast<UINT64>(m_renderSize.x),
						static_cast<UINT64>(m_renderSize.y),
						DXGI_FORMAT_R16G16B16A16_FLOAT,
						true,
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
				.Name = "Bloom Pass",
				.FilePath = L"Shaders\\public\\Bloom.hlsl",
			},
			/*ShaderPass
			{
				.Name = "Blit Pass",
				.FilePath = L"Shaders\\public\\Blit.hlsl",
				.VertexEntryPoint = L"BlitVS",
				.FragmentEntryPoint = L"BlitPS",
				.RasterizerDesc = GetRasterizerState(RasterizerState::NoCullNoMS),
				.BlendDesc = GetBlendState(BlendState::Disabled),
				.DepthStencilDesc = GetDepthState(DepthState::Disabled)
			}*/
		};

		m_pMaterial = std::make_unique<RenderMaterial>(m_shaderPasses);
		ShaderPasseIDs::BloomPassID = m_pMaterial->FindPassIndex("Bloom Pass");

		{
			RenderTargetDesc RTDesc = RenderTargetDesc
			{
				.m_renderTargetFormats = m_pBloomRT->GetFormat(),
				.m_numRenderTargets = 1,
				.m_depthStencilFormat = GetBufferManager()->GetCameraDepthRT()->GetFormat()
			};

			auto emplaceResult = m_PipelineStateObjects.try_emplace(ShaderPasseIDs::BloomPassID);
			if (emplaceResult.second)
			{
				emplaceResult.first->second = GetPSOManager()->GetGraphicsPipelineState(m_pMaterial.get(), ShaderPasseIDs::BloomPassID, RTDesc);
			}
		}
	}
	void BloomPass::Execute()
	{

	}
	void BloomPass::Render()
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "Bloom Pass");
		Execute();

		DoBloomPass();
	}

	void BloomPass::DoBloomPass()
	{
		m_pCommand->AddBarrier(*m_pBloomRT->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_pCommand->FlushBarrier();
		m_pCommand->ClearRenderTarget(*m_pBloomRT->GetTexture(), Color::Black);

		m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize));
		m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = m_PipelineStateObjects[ShaderPasseIDs::BloomPassID];
		pipelineStateData.m_renderTargets = { m_pBloomRT->GetTexture() };
		pipelineStateData.m_depthStencilTarget = GetBufferManager()->GetCameraDepthRT()->GetTexture();

		bool isReady = true;
		{
			if (m_pBloomRT->GetTexture() == nullptr || GetBufferManager()->GetCameraDepthRT()->GetTexture() == nullptr)
			{
				ThrowRuntimeError("null texture resource");
			}
			isReady &= m_pBloomRT->GetTexture()->GetIsReady();
			isReady &= GetBufferManager()->GetCameraDepthRT()->GetTexture()->GetIsReady();
		}
		if (isReady)
		{
			m_pCommand->SetPipeline(pipelineStateData);
			m_pCommand->SetPipelineResource(PER_PASS_SPACE, m_pMaterial->GetPassData(ShaderPasseIDs::BloomPassID).MeshResourceLayouts->m_spaces[PER_PASS_SPACE]);
			m_pCommand->SetPipelineResource(PER_FRAME_SPACE, GetRenderResource()->GetPerFrameBindResourceSpace());

			m_pCommand->Draw(3, 0);
		}

		m_pCommand->AddBarrier(*m_pBloomRT->GetTexture(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_pCommand->FlushBarrier();
	}

}