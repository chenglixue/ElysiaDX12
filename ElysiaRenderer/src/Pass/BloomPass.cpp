#include "stdafx.h"
#include "BloomPass.h"

#include "lib/DX12/DX12Device.h"
#include "lib/Utility/RenderTexture.h"
#include "RenderResource.h"

namespace ElysiaRenderer
{
	int BloomPass::ShaderPasseIDs::BloomPassID = -1;
	int BloomPass::ShaderPasseIDs::BlitPassID = -1;
	size_t BloomPass::ShaderIDs::g_DestTextureIndexID = -1;
	
	BloomPass::BloomPass(DX12Camera* pCamera) :
		BasePass(pCamera)
	{
		ShaderIDs::g_DestTextureIndexID = PropertyToID("g_DestTextureIndex");
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
						L"Bloom RT");
					break;
				}
				case HDRQuality::High:
				{
					m_pBloomRT = CreateRWRenderTexture
						(static_cast<UINT64>(m_renderSize.x),
						static_cast<UINT64>(m_renderSize.y),
						DXGI_FORMAT_R16G16B16A16_FLOAT,
						true,
						L"Bloom RT");
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
				.IsComputeShader = true,
				.ComputeEntryPoint = L"CS"
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
			auto emplaceResult = m_PipelineStateObjects.try_emplace(ShaderPasseIDs::BloomPassID);
			if (emplaceResult.second)
			{
				emplaceResult.first->second = GetPSOManager()->GetComputePipelineState(m_pMaterial.get(), ShaderPasseIDs::BloomPassID);
			}
		}
	}
	void BloomPass::Execute()
	{
		UpdatePSO();
	}
	void BloomPass::Render()
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "Bloom Pass");
		Execute();

		std::vector<int>

		DoBloomPass();
	}

	void BloomPass::DoBloomPass()
	{
		m_pCommand->AddBarrier(GetBufferManager()->GetCameraColorRT(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = m_PipelineStateObjects[ShaderPasseIDs::BloomPassID];

		bool isReady = true;
		{
			if (m_pBloomRT->GetTexture() == nullptr || GetBufferManager()->GetCameraDepthRT()->GetTexture() == nullptr)
			{
				ThrowRuntimeError("null texture resource");
			}
			isReady &= m_pBloomRT->GetTexture()->GetIsReady();
		}
		if (isReady)
		{
			m_pCommand->SetPipeline(pipelineStateData);
			m_pCommand->SetPipelineResource(PER_PASS_SPACE, m_pMaterial->GetPassData(ShaderPasseIDs::BloomPassID).MeshResourceLayouts->m_spaces[PER_PASS_SPACE]);
			m_pCommand->SetPipelineResource(PER_FRAME_SPACE, GetRenderResource()->GetPerFrameBindResourceSpace());
 
			m_pMaterial->SetConstantVariable(ShaderIDs::g_DestTextureIndexID, GetBufferManager()->GetCameraColorRT()->GetTexture()->GetResourceHeapIndex());
			m_pMaterial->SetConstantVariable("g_DestSize", GetScreenSize(m_renderSize));
			m_pMaterial->ApplyConstantData();
 
			m_pCommand->Dispatch(m_pBloomRT->GetWidth() / 8, m_pBloomRT->GetHeight() / 8, 1);
		}

		m_pCommand->AddBarrier(GetBufferManager()->GetCameraColorRT(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	void BloomPass::UpdatePSO()
	{
		
	}
}