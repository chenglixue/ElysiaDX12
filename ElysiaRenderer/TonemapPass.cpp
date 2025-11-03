#include "stdafx.h"
#include "TonemapPass.h"

#include "DX12Device.h"
#include "RenderTexture.h"

namespace ElysiaRenderer
{
	TonemapPass::~TonemapPass()
	{
		Dispose();
	}
	void TonemapPass::Dispose()
	{

	}

	void TonemapPass::Configure()
	{
		m_pTempRT = CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
			static_cast<UINT64>(m_renderSize.y),
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			L"Temp RT");

		ShaderCreateDesc shaderCreateDesc{};
		shaderCreateDesc.shaderName = L"Shaders\\public\\TonemapPass.hlsl";
		shaderCreateDesc.entryPoint = L"PS";
		shaderCreateDesc.shaderType = ShaderType::Pixel;
		m_pixelShader = std::move(GetDevice()->CreateShader(shaderCreateDesc));

		BindToShader();
		CreatePSO();
	}
	void TonemapPass::Execute()
	{
		BindToShader();
	}
	void TonemapPass::Render()
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "Tonemap Pass");

		Execute();
		{
			RenderResource::GetInstance().GetCBVPassParameter()->blitterTextureIndex = GetBufferManager()->GetCameraColorRT()->GetTexture()->GetResourceHeapIndex();
			GetBufferManager()->GetSingleConstantBuffer(PER_PASS_SPACE)->SetMappedData(RenderResource::GetInstance().GetCBVPassParameter(), sizeof(CBVMainPassParameter));

			m_pCommand->AddBarrier(*m_pTempRT->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
			m_pCommand->FlushBarrier();
			m_pCommand->ClearRenderTarget(*m_pTempRT->GetTexture(), Color(0, 0, 0, 0));

			m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize.x, m_renderSize.y));
			m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			PipelineInfo pipelineStateData{};
			pipelineStateData.m_pipelineStateObject = (*m_pGraphicsPipelineStates)[ShaderQueue::Blit].get();
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
				m_pCommand->SetPipelineResource(PER_PASS_SPACE, RenderResource::GetPerMainBindResourceSpace());

				m_pCommand->Draw(3, 0);
			}

			m_pCommand->AddBarrier(*m_pTempRT->GetTexture(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			m_pCommand->FlushBarrier();
		}

		{
			RenderResource::GetInstance().GetCBVPassParameter()->blitterTextureIndex = m_pTempRT->GetTexture()->GetResourceHeapIndex();
			GetBufferManager()->GetSingleConstantBuffer(PER_PASS_SPACE)->SetMappedData(RenderResource::GetInstance().GetCBVPassParameter(), sizeof(CBVMainPassParameter));

			auto cameraColorRT = GetBufferManager()->GetCameraColorRT();

			m_pCommand->AddBarrier(*cameraColorRT->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
			m_pCommand->FlushBarrier();
			m_pCommand->ClearRenderTarget(*cameraColorRT->GetTexture(), Color(0, 0, 0, 0));

			m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize.x, m_renderSize.y));
			m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			PipelineInfo pipelineStateData{};
			pipelineStateData.m_pipelineStateObject = m_TonemapPSO.get();
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
				m_pCommand->SetPipelineResource(PER_PASS_SPACE, RenderResource::GetPerMainBindResourceSpace());

				m_pCommand->Draw(3, 0);
			}

			m_pCommand->AddBarrier(*cameraColorRT->GetTexture(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			m_pCommand->FlushBarrier();
		}
	}

	void TonemapPass::BindToShader()
	{

	}

	void TonemapPass::CreatePSO()
	{
		{
			PipelineStateCreateDesc pipelineStateCreateDesc{};
			PipelineResourceLayout meshResourceLayout{};

			meshResourceLayout.m_spaces[PER_OBJECT_SPACE] = RenderResource::GetPerObjectBindResourceSpace();
			meshResourceLayout.m_spaces[PER_PASS_SPACE] = RenderResource::GetPerMainBindResourceSpace();

			pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
			pipelineStateCreateDesc.m_vertexShader = GetVertexShaders()[ShaderQueue::Blit][ShaderType::Vertex].get();
			pipelineStateCreateDesc.m_pixelShader = m_pixelShader.get();
			pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = 1;
			pipelineStateCreateDesc.m_renderTargetDesc.m_renderTargetFormats[0] = GetBufferManager()->GetCameraColorRT()->GetFormat();
			pipelineStateCreateDesc.m_renderTargetDesc.m_depthStencilFormat = GetBufferManager()->GetCameraDepthRT()->GetFormat();
			pipelineStateCreateDesc.m_depthStencilDesc = GetDepthState(DepthState::Disabled);
			pipelineStateCreateDesc.m_blendDesc = GetBlendState(BlendState::Disabled);
			pipelineStateCreateDesc.m_rasterDesc = GetRasterizerState(RasterizerState::NoCullNoMS);
			pipelineStateCreateDesc.m_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			m_TonemapPSO = std::move(GetDevice()->CreateGraphicsPipelineState(pipelineStateCreateDesc, meshResourceLayout));
		}
	}
}