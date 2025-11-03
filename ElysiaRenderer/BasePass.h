#pragma once
#include "stdafx.h"
#include "Common.h"
#include "RenderPassData.h"
#include "TextureManager.h"
#include "ModelImporter.h"
#include "RenderResource.h"
#include "RenderTexture.h"
#include "UserData.h"
#include "PIXHelper.h"

namespace ElysiaRenderer
{
	using namespace ElysiaModel;

	class RenderTexture;

	class BasePass
	{
	public:
		BasePass() : 
			m_renderSize(0, 0)
		{
			
		}
		virtual ~BasePass()
		{
			Dispose();
		}

		virtual void Setup(const RenderPassData& renderPassData)
		{
			m_renderSize = renderPassData.RenderSize;
			m_pCommand = renderPassData.pCommand;
			m_pGraphicsPipelineStates = renderPassData.pGraphicsPipelineStates;

			if ((*m_pGraphicsPipelineStates)[ShaderQueue::Blit] == nullptr)
			{
				PipelineResourceLayout meshResourceLayout{};
				PipelineStateCreateDesc pipelineStateCreateDesc{};

				meshResourceLayout.m_spaces[PER_OBJECT_SPACE] = RenderResource::GetPerObjectBindResourceSpace();
				meshResourceLayout.m_spaces[PER_PASS_SPACE] = RenderResource::GetPerMainBindResourceSpace();

				pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
				pipelineStateCreateDesc.m_vertexShader = GetVertexShaders()[ShaderQueue::Blit][ShaderType::Vertex].get();
				pipelineStateCreateDesc.m_pixelShader = GetPixelShaders()[ShaderQueue::Blit][ShaderType::Pixel].get();
				pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = 1;
				pipelineStateCreateDesc.m_renderTargetDesc.m_renderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
				pipelineStateCreateDesc.m_renderTargetDesc.m_depthStencilFormat = GetBufferManager()->GetCameraDepthRT()->GetFormat();
				pipelineStateCreateDesc.m_depthStencilDesc = GetDepthState(DepthState::Disabled);
				pipelineStateCreateDesc.m_blendDesc = GetBlendState(BlendState::Disabled);
				pipelineStateCreateDesc.m_rasterDesc = GetRasterizerState(RasterizerState::NoCullNoMS);
				pipelineStateCreateDesc.m_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

				(*m_pGraphicsPipelineStates)[ShaderQueue::Blit] = std::move(GetDevice()->CreateGraphicsPipelineState(pipelineStateCreateDesc, meshResourceLayout));

			}
			
			Configure();
		}
		virtual void Configure() = 0;
		virtual void Execute() = 0;
		virtual void Render()
		{
			Execute();
		}

		virtual void Dispose()
		{
		};

	protected:
		UINT2 m_renderSize;
		DX12GraphicsContext* m_pCommand = nullptr;
		std::unordered_map<UINT, std::unique_ptr<PipelineStateObject>>* m_pGraphicsPipelineStates = nullptr;
	};
}