#include "stdafx.h"
#include "GBufferPass.h"
#include "BufferManager.h"
#include "RenderResource.h"

namespace ElysiaRenderer
{
	int GBufferPass::ShaderPasseIDs::GBufferPassID = -1;

	GBufferPass::GBufferPass(DX12Camera* pCamera) :
		BasePass(pCamera)
	{

	}
	GBufferPass::~GBufferPass()
	{
		Dispose();
	}

	void GBufferPass::Configure()
	{
		CreateRTs();
		BindToShader();

		m_shaderPasses.emplace_back(ShaderPass
			{
				.Name = "GBuffer Pass",
				.FilePath = L"Shaders\\public\\GBuffer.hlsl",
				.RasterizerDesc = GetRasterizerState(RasterizerState::BackFaceCull),
				.BlendDesc = GetBlendState(BlendState::Disabled),
				.DepthStencilDesc = GetDepthState(DepthState::WritesEnabled)
			});

		m_pMaterial = std::move(std::make_unique<RenderMaterial>(m_shaderPasses));
		ShaderPasseIDs::GBufferPassID = m_pMaterial->FindPassIndex("GBuffer Pass");

		for (UINT meshIndex = 0; meshIndex < GetModelImporter()->GetMeshCount(); ++meshIndex)
		{
			auto& meshRenderer = GetModelImporter()->GetMeshRenderer(meshIndex);

			for (UINT frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; ++frameIndex)
			{
				auto objectBufferDesc = m_pMaterial->GetPassData(ShaderPasseIDs::GBufferPassID).ObjectBufferDesc;
				meshRenderer.m_objectBuffers[frameIndex] = std::move(GetDevice()->CreateBuffer(objectBufferDesc));

				auto materialBufferDesc = m_pMaterial->GetPassData(ShaderPasseIDs::GBufferPassID).MaterialBufferDesc;
				meshRenderer.m_materialBuffers[frameIndex] = std::move(GetDevice()->CreateBuffer(materialBufferDesc));
			}
		}

		{
			RenderTargetDesc RTDesc = RenderTargetDesc
			{
				.m_numRenderTargets = static_cast<UINT8>(m_GBufferRTs.size()),
				.m_depthStencilFormat = GetBufferManager()->GetCameraDepthRT()->GetFormat(),
			};
			for (int i = 0; i < m_GBufferRTs.size(); ++i)
			{
				RTDesc.m_renderTargetFormats[i] = m_GBufferRTs[i]->GetFormat();
			}

			auto emplaceResult = m_PipelineStateObjects.try_emplace(ShaderPasseIDs::GBufferPassID);
			if (emplaceResult.second)
			{
				emplaceResult.first->second = GetPSOManager()->GetGraphicsPipelineState(m_pMaterial.get(), ShaderPasseIDs::GBufferPassID, RTDesc);
			}
		}
	}

	void GBufferPass::Execute() 
	{
		UpdatePSO();
		m_pMaterial->SetConstantVariable("screenSize", GetScreenSize(Vector2(m_renderSize.x, m_renderSize.y)));
		m_pMaterial->SetConstantVariable("viewMatrix", m_pCamera->GetViewMat());
		m_pMaterial->SetConstantVariable("viewMatrix_I", m_pCamera->GetViewMat().Invert());
		m_pMaterial->SetConstantVariable("projMatrix", m_pCamera->GetProjMat());
		m_pMaterial->SetConstantVariable("projMatrix_I", m_pCamera->GetProjMat().Invert());
		m_pMaterial->SetConstantVariable("viewProjMatrix", m_pCamera->GetViewMat() * m_pCamera->GetProjMat());
		m_pMaterial->SetConstantVariable("viewProjMatrix_I", (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert());
		m_pMaterial->ApplyConstantData();
	}

	void GBufferPass::Render()
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "GBuffer Pass");

		Execute();

		auto cameraDepthRT = GetBufferManager()->GetCameraDepthRT();

		for (auto& RT : m_GBufferRTs)
		{
			m_pCommand->AddBarrier(RT.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
			m_pCommand->ClearRenderTarget(RT.get(), Color::Black);
		}
		m_pCommand->AddBarrier(cameraDepthRT, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_pCommand->ClearDepthStencilTarget(cameraDepthRT, 1.f, 0);

		m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize));
		m_pCommand->SetIndexBuffer(GetBufferManager()->GetIndexBufferView());
		m_pCommand->SetVertexBuffer(0, 1, const_cast<D3D12_VERTEX_BUFFER_VIEW&>(GetBufferManager()->GetVertexBufferView()));

		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = m_PipelineStateObjects[ShaderPasseIDs::GBufferPassID];
		pipelineStateData.m_renderTargets = std::move(GetGBuffers());
		pipelineStateData.m_depthStencilTarget = cameraDepthRT->GetTexture();

		bool isReady = true;
		{
			for (auto& RT : m_GBufferRTs)
			{
				if (RT->GetTexture() == nullptr)
				{
					ThrowRuntimeError("null texture resource");
				}
				isReady &= RT->GetTexture()->GetIsReady();
			}

			if (cameraDepthRT->GetTexture() == nullptr)
			{
				ThrowRuntimeError("null texture resource");
			}
			isReady &= cameraDepthRT->GetTexture()->GetIsReady();
		}
		if (isReady)
		{
			m_pCommand->SetPipeline(pipelineStateData);
			m_pCommand->SetPipelineResource(PER_PASS_SPACE, m_pMaterial->GetPassData(ShaderPasseIDs::GBufferPassID).MeshResourceLayouts->m_spaces[PER_PASS_SPACE]);
			m_pCommand->SetPipelineResource(PER_FRAME_SPACE, GetRenderResource()->GetPerFrameBindResourceSpace());

			UINT vertexStride = GetModelImporter()->GetVertexStride();

			for (UINT meshIndex = 0; meshIndex < GetModelImporter()->GetMeshCount(); ++meshIndex)
			{
				auto& meshRenderer = GetModelImporter()->GetMeshRenderer(meshIndex);
				const auto& mesh = meshRenderer.m_mesh;

				{
					std::unique_ptr<PipelineResourceSpace> pPipelineResourceSpace = std::make_unique<PipelineResourceSpace>();
					pPipelineResourceSpace->SetCBV(meshRenderer.m_objectBuffers[GetDevice()->GetFrameID()].get());
					pPipelineResourceSpace->Lock();
					if (m_pMaterial->GetPassData(ShaderPasseIDs::GBufferPassID).MeshResourceLayouts->m_spaces[PER_OBJECT_SPACE] != nullptr)
					{
						delete m_pMaterial->GetPassData(ShaderPasseIDs::GBufferPassID).MeshResourceLayouts->m_spaces[PER_OBJECT_SPACE];
						m_pMaterial->GetPassData(ShaderPasseIDs::GBufferPassID).MeshResourceLayouts->m_spaces[PER_OBJECT_SPACE] = nullptr;
					}
					m_pMaterial->GetPassData(ShaderPasseIDs::GBufferPassID).MeshResourceLayouts->m_spaces[PER_OBJECT_SPACE] = pPipelineResourceSpace.release();

					m_pMaterial->SetConstantVariable("worldMatrix", meshRenderer.m_CBVObjectParameter->worldMatrix);
					m_pMaterial->ApplyConstantData();
				}

				{
					auto& pUserData = UserData::GetInstance();

					std::unique_ptr<PipelineResourceSpace> pPipelineResourceSpace = std::make_unique<PipelineResourceSpace>();
					pPipelineResourceSpace->SetCBV(meshRenderer.m_materialBuffers[GetDevice()->GetFrameID()].get());
					pPipelineResourceSpace->Lock();
					if (m_pMaterial->GetPassData(ShaderPasseIDs::GBufferPassID).MeshResourceLayouts->m_spaces[PER_MATERIAL_SPACE] != nullptr)
					{
						delete m_pMaterial->GetPassData(ShaderPasseIDs::GBufferPassID).MeshResourceLayouts->m_spaces[PER_MATERIAL_SPACE];
					}
					m_pMaterial->GetPassData(ShaderPasseIDs::GBufferPassID).MeshResourceLayouts->m_spaces[PER_MATERIAL_SPACE] = pPipelineResourceSpace.release();

					m_pMaterial->SetConstantVariable("opacity", pUserData.Opacity);
					m_pMaterial->SetConstantVariable("cutoff", pUserData.Cutoff);
					m_pMaterial->SetConstantVariable("baseColorTexIndex", meshRenderer.m_CBVObjectParameter->baseColorTexIndex);
					m_pMaterial->SetConstantVariable("normalTexIndex", meshRenderer.m_CBVObjectParameter->normalTexIndex);
					m_pMaterial->SetConstantVariable("metallicTexIndex", meshRenderer.m_CBVObjectParameter->metallicTexIndex);
					m_pMaterial->SetConstantVariable("roughnessTexIndex", meshRenderer.m_CBVObjectParameter->roughnessTexIndex);
					m_pMaterial->SetConstantVariable("specularTexIndex", meshRenderer.m_CBVObjectParameter->specularTexIndex);

					m_pMaterial->SetConstantVariable("baseColorTint", pUserData.BaseColorTint);
					m_pMaterial->SetConstantVariable("ambientCubemapTint", pUserData.AmbientCubemapTint);
					m_pMaterial->SetConstantVariable("normalIntensity", pUserData.NormalIntensity);
					m_pMaterial->SetConstantVariable("metallicIntensity", pUserData.MetallicIntensity);
					m_pMaterial->SetConstantVariable("roughnessIntensity", pUserData.RoughnessIntensity);
					m_pMaterial->SetConstantVariable("ambientCubemapIntensity", pUserData.AmbientCubemapIntensity);
					m_pMaterial->SetConstantVariable("g_hasNormalTex", g_pModelImporter->GetMaterial(meshRenderer.m_mesh->materialIndex).hasNormal);
					m_pMaterial->ApplyConstantData();
				}

				m_pCommand->SetPipelineResource(PER_MATERIAL_SPACE, m_pMaterial->GetPassData(ShaderPasseIDs::GBufferPassID).MeshResourceLayouts->m_spaces[PER_MATERIAL_SPACE]);
				m_pCommand->SetPipelineResource(PER_OBJECT_SPACE, m_pMaterial->GetPassData(ShaderPasseIDs::GBufferPassID).MeshResourceLayouts->m_spaces[PER_OBJECT_SPACE]);

				auto startIndex = mesh->indexDataOffset / sizeof(UINT16);
				auto startVertex = mesh->vertexDataOffset / vertexStride;
				auto VertexCount = mesh->vertexCount;
				auto indexCount = mesh->indexCount;

				m_pCommand->Draw(indexCount, startVertex, static_cast<UINT>(startIndex));
			}
		}

		for (auto& RT : m_GBufferRTs)
		{
			m_pCommand->AddBarrier(RT.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
		m_pCommand->AddBarrier(cameraDepthRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ);
	}

	void GBufferPass::Dispose()
	{
		m_GBufferRTs.clear();
	}

	void GBufferPass::UpdatePSO()
	{
		
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
				L"GBuffer_5");

			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
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
		GetRenderResource()->GetCBVFrameVariable()->GBuffer0Index = m_GBufferRTs[GBufferIndex++]->GetTexture()->GetResourceHeapIndex();
		GetRenderResource()->GetCBVFrameVariable()->GBuffer1Index = m_GBufferRTs[GBufferIndex++]->GetTexture()->GetResourceHeapIndex();
		GetRenderResource()->GetCBVFrameVariable()->GBuffer2Index = m_GBufferRTs[GBufferIndex++]->GetTexture()->GetResourceHeapIndex();
		GetRenderResource()->GetCBVFrameVariable()->GBuffer3Index = m_GBufferRTs[GBufferIndex++]->GetTexture()->GetResourceHeapIndex();
		GetRenderResource()->GetCBVFrameVariable()->GBuffer4Index = m_GBufferRTs[GBufferIndex++]->GetTexture()->GetResourceHeapIndex();
		GetRenderResource()->GetCBVFrameVariable()->GBuffer5Index = m_GBufferRTs[GBufferIndex++]->GetTexture()->GetResourceHeapIndex();
		GetRenderResource()->GetCBVFrameVariable()->OpaqueDepthIndex = GetBufferManager()->GetCameraDepthRT()->GetTexture()->GetResourceHeapIndex();
		GetRenderResource()->GetCBVFrameVariable()->OpaqueColorIndex = GetBufferManager()->GetCameraColorRT()->GetTexture()->GetResourceHeapIndex();


	}
}