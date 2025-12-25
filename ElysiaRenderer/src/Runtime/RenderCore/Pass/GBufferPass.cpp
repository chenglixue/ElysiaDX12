#include "stdafx.h"
#include "GBufferPass.h"

#include "Editor/UserData.h"
#include "Programs/PIXHelper.h"

#include "Runtime/Core/DX12GraphicsContext.h"
#include "Runtime/Core/DX12TextureBuffer.h"
#include "Runtime/Core/DX12Shader.h"

#include "Runtime/Resource/Model/ModelManager.h"

#include "Runtime/RenderCore/RenderResource.h"
#include "Runtime/RenderCore/RenderTexture.h"
#include "Runtime/RenderCore/Material.h"
#include "Runtime/RenderCore/DX12Camera.h"
#include "Runtime/RenderCore/BufferManager.h"
#include "Runtime/RenderCore/RenderResource.h"
#include "Runtime/RenderCore/RenderTargetManager.h"
#include "Runtime/RenderCore/CameraManager.h"
#include "Runtime/RenderCore/PSOManager.h"
#include "Runtime/RenderCore/ShaderVariantManager.h"

namespace ElysiaRenderer
{
	int GBufferPass::ShaderPassIDs::GBufferPassID = -1;
	
	size_t GBufferPass::RenderTextureIDs::GBuffer0ID = PropertyToID("GBuffer_0");
	size_t GBufferPass::RenderTextureIDs::GBuffer1ID = PropertyToID("GBuffer_1");
	size_t GBufferPass::RenderTextureIDs::GBuffer2ID = PropertyToID("GBuffer_2");
	size_t GBufferPass::RenderTextureIDs::GBuffer3ID = PropertyToID("GBuffer_3");
	size_t GBufferPass::RenderTextureIDs::GBuffer4ID = PropertyToID("GBuffer_4");
	size_t GBufferPass::RenderTextureIDs::GBuffer5ID = PropertyToID("GBuffer_5");

	size_t GBufferPass::ShaderIDs::screenSize = PropertyToID("screenSize");
	size_t GBufferPass::ShaderIDs::viewMatrix = PropertyToID("viewMatrix");
	size_t GBufferPass::ShaderIDs::viewMatrix_I = PropertyToID("viewMatrix_I");
	size_t GBufferPass::ShaderIDs::projMatrix = PropertyToID("projMatrix");
	size_t GBufferPass::ShaderIDs::projMatrix_I = PropertyToID("projMatrix_I");
	size_t GBufferPass::ShaderIDs::viewProjMatrix = PropertyToID("viewProjMatrix");
	size_t GBufferPass::ShaderIDs::viewProjMatrix_I = PropertyToID("viewProjMatrix_I");
	size_t GBufferPass::ShaderIDs::worldMatrix = PropertyToID("worldMatrix");
	size_t GBufferPass::ShaderIDs::opacity = PropertyToID("opacity");
	size_t GBufferPass::ShaderIDs::cutoff = PropertyToID("cutoff");
	size_t GBufferPass::ShaderIDs::baseColorTexIndex = PropertyToID("baseColorTexIndex");
	size_t GBufferPass::ShaderIDs::normalTexIndex = PropertyToID("normalTexIndex");
	size_t GBufferPass::ShaderIDs::metallicTexIndex = PropertyToID("metallicTexIndex");
	size_t GBufferPass::ShaderIDs::roughnessTexIndex = PropertyToID("roughnessTexIndex");
	size_t GBufferPass::ShaderIDs::specularTexIndex = PropertyToID("specularTexIndex");
	size_t GBufferPass::ShaderIDs::baseColorTint = PropertyToID("baseColorTint");
	size_t GBufferPass::ShaderIDs::ambientCubemapTint = PropertyToID("ambientCubemapTint");
	size_t GBufferPass::ShaderIDs::normalIntensity = PropertyToID("normalIntensity");
	size_t GBufferPass::ShaderIDs::metallicIntensity = PropertyToID("metallicIntensity");
	size_t GBufferPass::ShaderIDs::roughnessIntensity = PropertyToID("roughnessIntensity");
	size_t GBufferPass::ShaderIDs::ambientCubemapIntensity = PropertyToID("ambientCubemapIntensity");
	size_t GBufferPass::ShaderIDs::g_hasNormalTex = PropertyToID("g_hasNormalTex");
	
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
		
		m_shaderPasses =
		{
			ShaderPass
			{
				.Name = "GBuffer Pass",
				.FilePath = L"Shaders\\public\\GBuffer.hlsl",
			},
		};
		m_pMaterial = std::move(std::make_unique<Material>(m_pDevice, m_shaderPasses));
		ShaderPassIDs::GBufferPassID = m_pMaterial->FindPassIndex("GBuffer Pass");

		UpdateVariant();
	}

	void GBufferPass::Render(ElysiaEngine::FrameContext context)
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "GBuffer Pass");

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
				RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::GBuffer0ID));

			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
		}

		// Metallic, Specular, Roughness, AO
		{
			auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
				static_cast<UINT64>(m_renderSize.y),
				DXGI_FORMAT_R8G8B8A8_UNORM,
				RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::GBuffer1ID));

			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
		}

		// Encode World Tangent, Anisotropy
		{
			auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
				static_cast<UINT64>(m_renderSize.y),
				DXGI_FORMAT_R8G8B8A8_UNORM,
				RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::GBuffer2ID));

			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
		}

		// Encode World Normal, per object data
		{
			auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
				static_cast<UINT64>(m_renderSize.y),
				DXGI_FORMAT_R10G10B10A2_UNORM,
				RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::GBuffer3ID));

			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
		}

		// Emission, opacity
		{
			auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
				static_cast<UINT64>(m_renderSize.y),
				DXGI_FORMAT_R10G10B10A2_UNORM,
				RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::GBuffer4ID));

			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
		}

		// Velocity
		{
			auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
				static_cast<UINT64>(m_renderSize.y),
				DXGI_FORMAT_R16G16B16A16_SNORM,
				RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::GBuffer5ID));

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
		UpdateGBufferPassVariant(ShaderPassIDs::GBufferPassID);
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
					.m_depthStencilFormat = m_pCameraDepthRT->GetFormat(),
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
		m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize));
		m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		
		auto& passData = m_pMaterial->GetPassData(passIndex);
		SetSpaceResource(passData, PER_PASS_SPACE); 
		SetSpaceResource(passData, PER_FRAME_SPACE);

		for (UINT i = 0; i < ModelManager::GetInstance().)
		m_pCommand->SetIndexBuffer(BufferManager::GetInstance().GetIndexBufferView()); 
		m_pCommand->SetVertexBuffer(0, 1, const_cast<D3D12_VERTEX_BUFFER_VIEW&>(BufferManager::GetInstance().GetVertexBufferView()));
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
		for (auto& RT : m_GBufferRTs)
		{
			m_pCommand->AddBarrier(RT, D3D12_RESOURCE_STATE_RENDER_TARGET, false);
		}
		m_pCommand->AddBarrier(m_pCameraDepthRT, D3D12_RESOURCE_STATE_DEPTH_WRITE, false);
		m_pCommand->FlushBarrier();
		
		for (auto& RT : m_GBufferRTs)
		{
			m_pCommand->ClearRenderTarget(RT, Color::Black);
		}
		m_pCommand->ClearDepthStencilTarget(m_pCameraDepthRT, 1.f, 0);
		
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

			if (m_pCameraDepthRT->GetTexture() == nullptr)
			{
				ThrowRuntimeError("null texture resource");
			}
			isReady &= m_pCameraDepthRT->GetTexture()->GetIsReady();
		}
		if (isReady)
		{
			PipelineInfo pipelineStateData{};
			pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(ShaderPassIDs::GBufferPassID).pPipelineStateObject;
			pipelineStateData.m_renderTargets = std::move(GetGBuffers());
			pipelineStateData.m_depthStencilTarget = m_pCameraDepthRT->GetTexture();
			m_pCommand->SetPipeline(pipelineStateData);
			DrawMesh(ShaderPassIDs::GBufferPassID);
		}

		for (auto& RT : m_GBufferRTs)
		{
			m_pCommand->AddBarrier(RT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, false);
		}
		m_pCommand->AddBarrier(m_pCameraDepthRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ, false);
		m_pCommand->FlushBarrier();
	}
}
