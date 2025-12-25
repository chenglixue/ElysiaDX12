#include "stdafx.h"
#include "SkyboxPass.h"

#include "Runtime/Core/DX12Device.h"
#include "Runtime/RenderCore/RenderTexture.h"


namespace ElysiaRenderer
{
	SkyboxPass::~SkyboxPass()
	{
		Dispose();
	}
	void SkyboxPass::Dispose()
	{

	}

	void SkyboxPass::Configure()
	{

		CreatePSO();
	}
	void SkyboxPass::Render(ElysiaEngine::FrameContext context)
	{
		/*Execute();

		auto cameraColorRT = GetBufferManager()->GetCameraColorRT();
		auto cameraDepthRT = GetBufferManager()->GetCameraDepthRT();

		m_pCommand->AddBarrier(*cameraColorRT->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_pCommand->FlushBarrier();

		m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize.x, m_renderSize.y));
		m_pCommand->SetIndexBuffer(GetBufferManager()->GetIndexBufferView());
		m_pCommand->SetVertexBuffer(0, 1, const_cast<D3D12_VERTEX_BUFFER_VIEW&>(GetBufferManager()->GetVertexBufferView()));

		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = (*m_pGraphicsPipelineStates)[ShaderQueue::GBuffer].get();
		pipelineStateData.m_renderTargets.emplace_back(cameraColorRT->GetTexture());
		pipelineStateData.m_depthStencilTarget = cameraDepthRT->GetTexture();

		bool isReady = true;
		{

			if (cameraDepthRT->GetTexture() == nullptr || cameraColorRT->GetTexture())
			{
				ThrowRuntimeError("null texture resource");
			}
			isReady &= cameraDepthRT->GetTexture()->GetIsReady();
			isReady &= cameraColorRT->GetTexture()->GetIsReady();
		}
		if (isReady)
		{
			m_pCommand->SetPipeline(pipelineStateData);
			m_pCommand->SetPipelineResource(PER_PASS_SPACE, RenderResource::GetPerMainBindResourceSpace());

			UINT vertexStride = GetModelImporter()->GetVertexStride();

			for (UINT meshIndex = 0; meshIndex < GetModelImporter()->GetMeshCount(); ++meshIndex)
			{
				const auto& meshRenderer = GetModelImporter()->GetMeshRenderer(meshIndex);
				const auto& mesh = meshRenderer.m_mesh;

				auto objectContantBuffer = GetBufferManager()->GetMutilConstantBuffer(PER_OBJECT_SPACE, GetDevice()->GetFrameID(), meshIndex);
				RenderResource::GetPerObjectBindResourceSpace()->SetCBV(objectContantBuffer);
				m_pCommand->SetPipelineResource(PER_OBJECT_SPACE, RenderResource::GetPerObjectBindResourceSpace());

				auto startIndex = mesh->indexDataOffset / sizeof(UINT16);
				auto startVertex = mesh->vertexDataOffset / vertexStride;
				auto VertexCount = mesh->vertexCount;
				auto indexCount = mesh->indexCount;

				m_pCommand->Draw(indexCount, startVertex, startIndex);
			}
		}*/

		/*auto& currBackBuffer = m_device->GetCurrBackBuffer();

		auto pipelineStateData = CreatePipelineStateData(m_graphicsPipelineStates[ShaderQueue::Skybox].get(),
			std::vector<DX12TextureResource*>{ &currBackBuffer },
			m_pCameraDepthBuffer.get());
		m_graphicsContext->SetPipeline(pipelineStateData);

		{
			SetPipelineResource(m_objectCBVIndex, CBVPassParameterType::Main);
			DrawCommand(m_objectCBVIndex++);
		}*/
	}

	void SkyboxPass::CreatePSO()
	{
		/// Skybox PSO
		//PipelineResourceLayout meshResourceLayout{};
		//PipelineStateCreateDesc pipelineStateCreateDesc{};

		//meshResourceLayout.m_spaces[PER_OBJECT_SPACE] = RenderResource::GetPerObjectBindResourceSpace();
		//meshResourceLayout.m_spaces[PER_PASS_SPACE] = RenderResource::GetPerMainBindResourceSpace();

		//pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
		//pipelineStateCreateDesc.m_vertexShader = GetVertexShaders()[ShaderQueue::Skybox][ShaderType::Vertex].get();;
		//pipelineStateCreateDesc.m_pixelShader = GetPixelShaders()[ShaderQueue::Skybox][ShaderType::Pixel].get();
		//pipelineStateCreateDesc.m_inputElementDesc = g_inputElementDescs;
		//pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = 1;
		//pipelineStateCreateDesc.m_renderTargetDesc.m_renderTargetFormats[0] = GetBufferManager()->GetCameraColorRT()->GetFormat();
		//pipelineStateCreateDesc.m_depthStencilDesc = GetDepthState(DepthState::Enabled);
		//pipelineStateCreateDesc.m_rasterDesc = GetRasterizerState(RasterizerState::NoCullNoMS);
		//pipelineStateCreateDesc.m_blendDesc = GetBlendState(BlendState::Disabled);
		//// let cubemap z = 1 pass z-test, otherwise it'll be failed in z-test because data of zbuffer is 1
		////pipelineStateCreateDesc.m_depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		//(*m_pGraphicsPipelineStates)[ShaderQueue::Skybox] = std::move(GetDevice()->CreateGraphicsPipelineState(pipelineStateCreateDesc, meshResourceLayout));
	}

	void SkyboxPass::UpdatePSO()
	{
		
	}
}