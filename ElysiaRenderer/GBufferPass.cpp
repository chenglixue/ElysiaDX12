#include "GBufferPass.h"

namespace ElysiaRenderer
{
	GBufferPass::~GBufferPass()
	{
		Dispose();
	}

	void GBufferPass::Configure()
	{
		AddShader(ShaderQueue::GBuffer, L"Shaders\\public\\GBuffer.hlsl", L"VS", ShaderType::Vertex);
		AddShader(ShaderQueue::GBuffer, L"Shaders\\public\\GBuffer.hlsl", L"PS", ShaderType::Pixel);

		CreateRTs();
		BindToShader();
		CreatePSO();
	}

	void GBufferPass::Execute()
	{
		GetBufferManager()->GetSingleConstantBuffer(PER_PASS_SPACE)->SetMappedData(RenderResource::GetInstance().GetCBVPassParameter(), sizeof(CBVMainPassParameter));
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
		pipelineStateData.m_pipelineStateObject = (*m_pGraphicsPipelineStates)[ShaderQueue::GBuffer].get();
		pipelineStateData.m_renderTargets = std::move(GetGBuffers());
		pipelineStateData.m_depthStencilTarget = m_pDepthRT->GetTexture();

		bool isReady = true;
		{
			auto texResources = TextureManager::GetInstance().GetTextureResources();
			for (size_t i = 0; i < texResources.size(); ++i)
			{
				if (texResources[i] == nullptr)
				{
					ThrowRuntimeError("nullptr");
				}
				isReady &= texResources[i]->GetIsReady();
			}
			for (auto& RT : m_GBufferRTs)
			{
				if (RT->GetTexture() == nullptr)
				{
					ThrowRuntimeError("null texture resource");
				}
				isReady &= RT->GetTexture()->GetIsReady();
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

		for (auto& RT : m_GBufferRTs)
		{
			m_pCommand->AddBarrier(*RT->GetTexture(), D3D12_RESOURCE_STATE_GENERIC_READ);
		}
		m_pCommand->FlushBarrier();
	}

	void GBufferPass::Dispose()
	{
		m_GBufferRTs.clear();
	}

	void GBufferPass::CreateRTs()
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
			m_pDepthRT = CreateRenderTexture(
				static_cast<UINT64>(m_renderSize.x),
				static_cast<UINT64>(m_renderSize.y),
				DXGI_FORMAT_D24_UNORM_S8_UINT,
				true,
				L"GBuffer Depth RT");
		}
	}

	std::vector<DX12TextureResource*> GBufferPass::GetGBuffers()
	{
		std::vector<DX12TextureResource*> temp{};
		temp.reserve(m_GBufferRTs.size());
		for (auto& RT : m_GBufferRTs)
		{
			temp.emplace_back(RT->GetTexture());
		}

		return temp;
	}

	void GBufferPass::BindToShader()
	{
		int GBufferIndex = 0;
		RenderResource::GetInstance().GetCBVPassParameter()->GBuffer0Index = m_GBufferRTs[GBufferIndex++]->GetTexture()->GetResourceHeapIndex();
		RenderResource::GetInstance().GetCBVPassParameter()->GBuffer1Index = m_GBufferRTs[GBufferIndex++]->GetTexture()->GetResourceHeapIndex();
		RenderResource::GetInstance().GetCBVPassParameter()->GBuffer2Index = m_GBufferRTs[GBufferIndex++]->GetTexture()->GetResourceHeapIndex();
		RenderResource::GetInstance().GetCBVPassParameter()->GBuffer3Index = m_GBufferRTs[GBufferIndex++]->GetTexture()->GetResourceHeapIndex();
		RenderResource::GetInstance().GetCBVPassParameter()->GBuffer4Index = m_GBufferRTs[GBufferIndex++]->GetTexture()->GetResourceHeapIndex();
		RenderResource::GetInstance().GetCBVPassParameter()->GBuffer5Index = m_GBufferRTs[GBufferIndex++]->GetTexture()->GetResourceHeapIndex();
	}

	void GBufferPass::CreatePSO()
	{
		PipelineStateCreateDesc pipelineStateCreateDesc{};
		PipelineResourceLayout meshResourceLayout{};

		meshResourceLayout.m_spaces[PER_OBJECT_SPACE] = RenderResource::GetPerObjectBindResourceSpace();
		meshResourceLayout.m_spaces[PER_PASS_SPACE] = RenderResource::GetPerMainBindResourceSpace();

		pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
		pipelineStateCreateDesc.m_vertexShader = GetVertexShaders()[ShaderQueue::GBuffer][ShaderType::Vertex].get();
		pipelineStateCreateDesc.m_pixelShader = GetPixelShaders()[ShaderQueue::GBuffer][ShaderType::Pixel].get();
		pipelineStateCreateDesc.m_inputElementDesc = g_inputElementDescs;
		pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = m_GBufferRTs.size();
		for (int i = 0; i < m_GBufferRTs.size(); ++i)
		{
			pipelineStateCreateDesc.m_renderTargetDesc.m_renderTargetFormats[i] = m_GBufferRTs[i]->GetFormat();
		}
		pipelineStateCreateDesc.m_depthStencilDesc.DepthEnable = TRUE;
		pipelineStateCreateDesc.m_renderTargetDesc.m_depthStencilFormat = m_pDepthRT->GetFormat();
		pipelineStateCreateDesc.m_depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		(*m_pGraphicsPipelineStates)[ShaderQueue::GBuffer] = std::move(GetDevice()->CreateGraphicsPipelineState(pipelineStateCreateDesc, meshResourceLayout));
	}
}