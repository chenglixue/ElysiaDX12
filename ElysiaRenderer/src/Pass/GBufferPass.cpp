#include "stdafx.h"
#include "GBufferPass.h"
#include "Manager/BufferManager.h"
#include "RenderResource.h"
#include "Manager/RenderTargetManager.h"

namespace ElysiaRenderer
{
	int GBufferPass::ShaderPasseIDs::GBufferPassID = -1;
	
	size_t GBufferPass::RenderTextureIDs::GBufferPass0ID = SIZE_MAX;
	size_t GBufferPass::RenderTextureIDs::GBufferPass1ID = SIZE_MAX;
	size_t GBufferPass::RenderTextureIDs::GBufferPass2ID = SIZE_MAX;
	size_t GBufferPass::RenderTextureIDs::GBufferPass3ID = SIZE_MAX;
	size_t GBufferPass::RenderTextureIDs::GBufferPass4ID = SIZE_MAX;
	size_t GBufferPass::RenderTextureIDs::GBufferPass5ID = SIZE_MAX;

	size_t GBufferPass::ShaderIDs::screenSize = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::viewMatrix = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::viewMatrix_I = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::projMatrix = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::projMatrix_I = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::viewProjMatrix = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::viewProjMatrix_I = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::worldMatrix = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::opacity = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::cutoff = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::baseColorTexIndex = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::normalTexIndex = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::metallicTexIndex = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::roughnessTexIndex = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::specularTexIndex = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::baseColorTint = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::ambientCubemapTint = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::normalIntensity = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::metallicIntensity = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::roughnessIntensity = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::ambientCubemapIntensity = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::g_hasNormalTex = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::GBuffer0Index = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::GBuffer1Index = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::GBuffer2Index = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::GBuffer3Index = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::GBuffer4Index = SIZE_MAX;
	size_t GBufferPass::ShaderIDs::GBuffer5Index = SIZE_MAX;
	
	GBufferPass::GBufferPass(DX12Camera* pCamera) :
		BasePass(pCamera)
	{
		RenderTextureIDs::GBufferPass0ID = PropertyToID("GBuffer_0");
		RenderTextureIDs::GBufferPass1ID = PropertyToID("GBuffer_1");
		RenderTextureIDs::GBufferPass2ID = PropertyToID("GBuffer_2");
		RenderTextureIDs::GBufferPass3ID = PropertyToID("GBuffer_3");
		RenderTextureIDs::GBufferPass4ID = PropertyToID("GBuffer_4");
		RenderTextureIDs::GBufferPass5ID = PropertyToID("GBuffer_5");
		
		ShaderIDs::screenSize = PropertyToID("screenSize");
		ShaderIDs::viewMatrix = PropertyToID("viewMatrix");
		ShaderIDs::viewMatrix_I = PropertyToID("viewMatrix_I");
		ShaderIDs::projMatrix = PropertyToID("projMatrix");
		ShaderIDs::projMatrix_I = PropertyToID("projMatrix_I");
		ShaderIDs::viewProjMatrix = PropertyToID("viewProjMatrix");
		ShaderIDs::viewProjMatrix_I = PropertyToID("viewProjMatrix_I");
		ShaderIDs::worldMatrix = PropertyToID("worldMatrix");
		ShaderIDs::opacity = PropertyToID("opacity");
		ShaderIDs::cutoff = PropertyToID("cutoff");
		ShaderIDs::baseColorTexIndex = PropertyToID("baseColorTexIndex");
		ShaderIDs::normalTexIndex = PropertyToID("normalTexIndex");
		ShaderIDs::metallicTexIndex = PropertyToID("metallicTexIndex");
		ShaderIDs::roughnessTexIndex = PropertyToID("roughnessTexIndex");
		ShaderIDs::specularTexIndex = PropertyToID("specularTexIndex");
		ShaderIDs::baseColorTint = PropertyToID("baseColorTint");
		ShaderIDs::ambientCubemapTint = PropertyToID("ambientCubemapTint");
		ShaderIDs::normalIntensity = PropertyToID("normalIntensity");
		ShaderIDs::metallicIntensity = PropertyToID("metallicIntensity");
		ShaderIDs::roughnessIntensity = PropertyToID("roughnessIntensity");
		ShaderIDs::ambientCubemapIntensity = PropertyToID("ambientCubemapIntensity");
		ShaderIDs::g_hasNormalTex = PropertyToID("g_hasNormalTex");
	}
	GBufferPass::~GBufferPass()
	{
		Dispose();
	}

	void GBufferPass::Configure()
	{
		CreateRTs();
		
		m_shaderPasses =
		{
			ShaderPass
			{
				.Name = "GBuffer Pass",
				.FilePath = L"Shaders\\public\\GBuffer.hlsl",
			},
		};
		m_pMaterial = std::move(std::make_unique<Material>(m_pDevice, m_shaderPasses));
		ShaderPasseIDs::GBufferPassID = m_pMaterial->FindPassIndex("GBuffer Pass");

		UpdateVariant();

		RenderResource::GetInstance().GetCBVFrameVariable()->GBuffer0Index = m_GBufferRTs[0]->GetResourceHeapIndex();
		RenderResource::GetInstance().GetCBVFrameVariable()->GBuffer1Index = m_GBufferRTs[1]->GetResourceHeapIndex();
		RenderResource::GetInstance().GetCBVFrameVariable()->GBuffer2Index = m_GBufferRTs[2]->GetResourceHeapIndex();
		RenderResource::GetInstance().GetCBVFrameVariable()->GBuffer3Index = m_GBufferRTs[3]->GetResourceHeapIndex();
		RenderResource::GetInstance().GetCBVFrameVariable()->GBuffer4Index = m_GBufferRTs[4]->GetResourceHeapIndex();
		RenderResource::GetInstance().GetCBVFrameVariable()->GBuffer5Index = m_GBufferRTs[5]->GetResourceHeapIndex();
	}

	void GBufferPass::Execute()
	{
		UpdatePSO();
		m_pMaterial->SetFloat4(ShaderIDs::screenSize, GetScreenSize(Vector2(m_renderSize.x, m_renderSize.y)));
		m_pMaterial->SetMatrix(ShaderIDs::viewMatrix, m_pCamera->GetViewMat());
		m_pMaterial->SetMatrix(ShaderIDs::viewMatrix_I, m_pCamera->GetViewMat().Invert());
		m_pMaterial->SetMatrix(ShaderIDs::projMatrix, m_pCamera->GetProjMat());
		m_pMaterial->SetMatrix(ShaderIDs::projMatrix_I, m_pCamera->GetProjMat().Invert());
		m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix, m_pCamera->GetViewMat() * m_pCamera->GetProjMat());
		m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix_I, (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert());
		m_pMaterial->SetFloat3(ShaderIDs::baseColorTint, UserData::GetInstance().BaseColorTint);
		m_pMaterial->SetFloat3(ShaderIDs::ambientCubemapTint, UserData::GetInstance().AmbientCubemapTint);
		m_pMaterial->SetFloat(ShaderIDs::normalIntensity, UserData::GetInstance().NormalIntensity);
		m_pMaterial->SetFloat(ShaderIDs::metallicIntensity, UserData::GetInstance().MetallicIntensity);
		m_pMaterial->SetFloat(ShaderIDs::roughnessIntensity, UserData::GetInstance().RoughnessIntensity);
		m_pMaterial->SetFloat(ShaderIDs::ambientCubemapIntensity, UserData::GetInstance().AmbientCubemapIntensity);
	}

	void GBufferPass::Render()
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "GBuffer Pass");

		Execute();

		DrawGBufferPass();
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
		// Base Color , ShadingModel
		{
			auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
				static_cast<UINT64>(m_renderSize.y),
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
				RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::GBufferPass0ID));

			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
		}

		// Metallic, Specular, Roughness, AO
		{
			auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
				static_cast<UINT64>(m_renderSize.y),
				DXGI_FORMAT_R8G8B8A8_UNORM,
				RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::GBufferPass1ID));

			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
		}

		// Encode World Tangent, Anisotropy
		{
			auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
				static_cast<UINT64>(m_renderSize.y),
				DXGI_FORMAT_R8G8B8A8_UNORM,
				RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::GBufferPass2ID));

			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
		}

		// Encode World Normal, per object data
		{
			auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
				static_cast<UINT64>(m_renderSize.y),
				DXGI_FORMAT_R10G10B10A2_UNORM,
				RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::GBufferPass3ID));

			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
		}

		// Emission, opacity
		{
			auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
				static_cast<UINT64>(m_renderSize.y),
				DXGI_FORMAT_R10G10B10A2_UNORM,
				RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::GBufferPass4ID));

			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
		}

		// Velocity
		{
			auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
				static_cast<UINT64>(m_renderSize.y),
				DXGI_FORMAT_R16G16B16A16_SNORM,
				RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::GBufferPass5ID));

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

	void GBufferPass::UpdateVariant()
	{
		UpdateGBufferPassVariant(ShaderPasseIDs::GBufferPassID);
	}
	void GBufferPass::UpdateGBufferPassVariant(UINT passIndex)
	{
		std::vector<std::wstring> enableKeywords{};
		 
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
					.m_numRenderTargets = static_cast<UINT8>(m_GBufferRTs.size()),
					.m_depthStencilFormat = BufferManager::GetInstance().GetCameraDepthRT()->GetFormat(),
				};
				for (int i = 0; i < m_GBufferRTs.size(); ++i)
				{
					RTDesc.m_renderTargetFormats[i] = m_GBufferRTs[i]->GetFormat();
				}
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

	void GBufferPass::DrawMesh(UINT passIndex)
	{
		m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pCommand->SetIndexBuffer(BufferManager::GetInstance().GetIndexBufferView()); 
		m_pCommand->SetVertexBuffer(0, 1, const_cast<D3D12_VERTEX_BUFFER_VIEW&>(BufferManager::GetInstance().GetVertexBufferView()));
		
		auto& passData = m_pMaterial->GetPassData(passIndex);
		SetSpaceResource(passData, PER_PASS_SPACE); 
		SetSpaceResource(passData, PER_FRAME_SPACE);
		
		for (UINT meshIndex = 0; meshIndex < GetModelImporter()->GetMeshCount(); ++meshIndex)
		{
			const auto& meshRenderer = GetModelImporter()->GetMeshRenderer(meshIndex);
			const auto& mesh = meshRenderer.m_mesh;

			m_pMaterial->SetMatrix(ShaderIDs::worldMatrix, meshRenderer.m_worldMatrix);
			m_pMaterial->SetUInt(ShaderIDs::baseColorTexIndex, meshRenderer.m_CBVObjectParameter->baseColorTexIndex);
			m_pMaterial->SetUInt(ShaderIDs::normalTexIndex, meshRenderer.m_CBVObjectParameter->normalTexIndex);
			m_pMaterial->SetUInt(ShaderIDs::metallicTexIndex, meshRenderer.m_CBVObjectParameter->metallicTexIndex);
			m_pMaterial->SetUInt(ShaderIDs::roughnessTexIndex, meshRenderer.m_CBVObjectParameter->roughnessTexIndex);
			m_pMaterial->SetUInt(ShaderIDs::specularTexIndex, meshRenderer.m_CBVObjectParameter->specularTexIndex);
			m_pMaterial->SetFloat(ShaderIDs::cutoff, meshRenderer.m_CBVObjectParameter->cutoff);
			m_pMaterial->SetFloat(ShaderIDs::opacity, meshRenderer.m_CBVObjectParameter->opacity);
			m_pMaterial->SetBool(ShaderIDs::g_hasNormalTex, meshRenderer.m_CBVObjectParameter->hasNormalTex);
			
			SetSpaceResource(passData, PER_OBJECT_SPACE);
			SetSpaceResource(passData, PER_MATERIAL_SPACE);

			auto startIndex = mesh->indexDataOffset / sizeof(UINT16);
			auto startVertex = mesh->vertexDataOffset / GetModelImporter()->GetVertexStride();
			auto indexCount = mesh->indexCount;

			m_pCommand->Draw(indexCount, startVertex, static_cast<UINT>(startIndex));
		}
	}

	void GBufferPass::DrawGBufferPass()
	{
		auto cameraDepthRT = BufferManager::GetInstance().GetCameraDepthRT();

		for (auto& RT : m_GBufferRTs)
		{
			m_pCommand->AddBarrier(RT, D3D12_RESOURCE_STATE_RENDER_TARGET, false);
		}
		m_pCommand->AddBarrier(cameraDepthRT, D3D12_RESOURCE_STATE_DEPTH_WRITE, false);
		m_pCommand->FlushBarrier();
		
		for (auto& RT : m_GBufferRTs)
		{
			m_pCommand->ClearRenderTarget(RT, Color::Black);
		}
		m_pCommand->ClearDepthStencilTarget(cameraDepthRT, 1.f, 0);
		
		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(ShaderPasseIDs::GBufferPassID).pPipelineStateObject;
		pipelineStateData.m_renderTargets = std::move(GetGBuffers());
		pipelineStateData.m_depthStencilTarget = cameraDepthRT->GetTexture();
		m_pCommand->SetPipeline(pipelineStateData);

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
			DrawMesh(ShaderPasseIDs::GBufferPassID);
		}

		for (auto& RT : m_GBufferRTs)
		{
			m_pCommand->AddBarrier(RT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, false);
		}
		m_pCommand->AddBarrier(cameraDepthRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ, false);
		m_pCommand->FlushBarrier();
	}
}
