#include "GBufferPass.h"

namespace ElysiaRenderer
{
	GBufferPass::~GBufferPass()
	{
		Dispose();
	}

	void GBufferPass::Configure()
	{
		RenderTextureDesc RTCreateDesc{};

		// Base Color , ShadingModel
		{
			auto pGBufferRT = CreateRenderTexture(static_cast<UINT64>(m_renderSize.x), 
				static_cast<UINT64>(m_renderSize.y),
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
				L"GBuffer_0");

			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
		}
			
		// Metallic, Specular, Roughness, AO
		{
			auto pGBufferRT = CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
				static_cast<UINT64>(m_renderSize.y),
				DXGI_FORMAT_R8G8B8A8_UNORM,
				L"GBuffer_1");

			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
		}

		// Encode World Tangent, Anisotropy
		{
			auto pGBufferRT = CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
				static_cast<UINT64>(m_renderSize.y),
				DXGI_FORMAT_R8G8B8A8_UNORM,
				L"GBuffer_2");

			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
		}

		// Encode World Normal, per object data
		{
			auto pGBufferRT = CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
				static_cast<UINT64>(m_renderSize.y),
				DXGI_FORMAT_R10G10B10A2_UNORM,
				L"GBuffer_3");

			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
		}

		// Emission, opacity
		{
			auto pGBufferRT = CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
				static_cast<UINT64>(m_renderSize.y),
				DXGI_FORMAT_R10G10B10A2_UNORM,
				L"GBuffer_4");

			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
		}

		// Velocity
		{
			auto pGBufferRT = CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
				static_cast<UINT64>(m_renderSize.y),
				DXGI_FORMAT_R16G16B16A16_SNORM,
				L"GBuffer_4");

			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
		}

		// Depth
		{
			m_depthRT = CreateRenderTexture(
				static_cast<UINT64>(m_renderSize.x),
				static_cast<UINT64>(m_renderSize.y),
				DXGI_FORMAT_D24_UNORM_S8_UINT,
				true,
				L"GBuffer Depth RT");
		}
	}

	void GBufferPass::Execute()
	{

	}

	void GBufferPass::Render()
	{
		Execute();

		for (auto& RT : m_GBufferRTs)
		{
			m_pCommand->AddBarrier(*RT->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
			m_pCommand->ClearRenderTarget(*RT->GetTexture(), Color(1, 1, 1));
		}
		m_pCommand->FlushBarrier();

		m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(static_cast<UINT>(GetDevice()->GetScreenSize().x), static_cast<UINT>(GetDevice()->GetScreenSize().y)));

		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = (*m_pGraphicsPipelineStates)[ShaderQueue::Shadow].get();
		pipelineStateData.m_renderTargets = std::move(GetGBuffers());
		pipelineStateData.m_depthStencilTarget = m_depthRT->GetTexture();

		bool isReady = true;
		{
			auto texResources = TextureManager::GetInstance().GetTextureResources();
			for (size_t i = 0; i < texResources.size(); ++i)
			{
				if (texResources[i] == nullptr)
				{
					ThrowRuntimeError("nullptr");;
				}
				isReady &= texResources[i]->GetIsReady();
			}
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
		}
	}

	void GBufferPass::Dispose()
	{
		m_GBufferRTs.clear();
	}
}