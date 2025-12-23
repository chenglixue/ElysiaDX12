#include "stdafx.h"
#include "ShadowPass.h"

#include "GBufferPass.h"
#include "lib/DX12/DX12Material.h"
#include "RenderResource.h" 
#include "DX12/UploadRingBuffer.h"
#include "lib/Utility/PIXHelper.h"
#include "Manager/PSOManager.h"
#include "lib/Utility/SobolSequenceGenerator.h"
#include "Manager/CameraManager.h"
#include "Manager/RenderTargetManager.h"
#include "Utility/RenderHelper.h"

namespace ElysiaRenderer
{
	int ShadowPass::ShaderPassIDs::ShadowCastPassID = -1;
	
	size_t ShadowPass::RenderTextureIDs::ShadowRTID = PropertyToID("Shadow RT");
	
	size_t ShadowPass::ShaderIDs::shadowNearZ = PropertyToID("shadowNearZ");
	size_t ShadowPass::ShaderIDs::shadowFarZ = PropertyToID("shadowFarZ");
	size_t ShadowPass::ShaderIDs::shadowDepthBias = PropertyToID("shadowDepthBias");
	size_t ShadowPass::ShaderIDs::shadowSlopeDepthBias = PropertyToID("shadowSlopeDepthBias");
	size_t ShadowPass::ShaderIDs::shadowMaxSlopeDepthBias = PropertyToID("shadowMaxSlopeDepthBias");
	size_t ShadowPass::ShaderIDs::g_sobolSequence = PropertyToID("g_sobolSequence");
	size_t ShadowPass::ShaderIDs::worldMatrix = PropertyToID("worldMatrix");
	size_t ShadowPass::ShaderIDs::baseColorTexIndex = PropertyToID("baseColorTexIndex");
	size_t ShadowPass::ShaderIDs::opacity = PropertyToID("opacity");
	size_t ShadowPass::ShaderIDs::cutoff = PropertyToID("cutoff");

	ShadowPass::ShadowPass(DX12Camera* pCamera) :
		BasePass(pCamera)
	{
	};
	ShadowPass::~ShadowPass()
	{
		Dispose();
	}
	void ShadowPass::Dispose()
	{

	}
 
	void ShadowPass::Configure()
	{
		m_pMainLight = LightManager::GetInstance().GetMainLight();
		
		m_shaderPasses =
		{ 
			ShaderPass
			{ 
				.Name = "Shadow Cast Pass",
				.FilePath = L"Shaders\\public\\Shadow.hlsl",
			} 
		};
		m_pMaterial = std::move(std::make_unique<Material>(m_pDevice, m_shaderPasses));
		ShaderPassIDs::ShadowCastPassID = m_pMaterial->FindPassIndex("Shadow Cast Pass");

		m_sobolSqeuences = Create2DSobolSqeuence(64);
		UpdateVariant();
	}
	void ShadowPass::Execute()
	{
		m_pMaterial->SetFloat(ShaderIDs::shadowNearZ, LightManager::GetInstance().GetMainShadow()->GetNearZ());
		m_pMaterial->SetFloat(ShaderIDs::shadowFarZ, LightManager::GetInstance().GetMainShadow()->GetFarZ());
		m_pMaterial->SetFloat(ShaderIDs::shadowDepthBias, UserData::GetInstance().shadowDepthBias / 100);
		m_pMaterial->SetFloat(ShaderIDs::shadowSlopeDepthBias, UserData::GetInstance().shadowSlopeDepthBias / 100);
		m_pMaterial->SetFloat(ShaderIDs::shadowMaxSlopeDepthBias, UserData::GetInstance().shadowMaxSlopeDepthBias / 100);
		m_pMaterial->SetVector2Array(ShaderIDs::g_sobolSequence, m_sobolSqeuences);
	} 
	void ShadowPass::Render()
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "Shadow Pass");

		Execute();

		DrawShadowPass();
	}
	
	void ShadowPass::UpdatePSO()
	{
		
	}
	void ShadowPass::UpdateVariant()
	{
		UpdateShadowPassVariant(ShaderPassIDs::ShadowCastPassID);
	}
	void ShadowPass::UpdateShadowPassVariant(UINT passIndex)
	{
		std::vector<std::wstring> enableKeywords{};
		
		switch (UserData::GetInstance().shadowQuality)
		{
			case ShadowQuality::Low:
				{
					enableKeywords.emplace_back(L"SHADOW_QUALITY_LOW");
					break;
				}
			case ShadowQuality::Middle:
				{
					enableKeywords.emplace_back(L"SHADOW_QUALITY_MIDDLE");
					break;
				}
			case ShadowQuality::High:
				{
					enableKeywords.emplace_back(L"SHADOW_QUALITY_HIGH");
					break;
				}
			case ShadowQuality::VeryHigh:
				{
					enableKeywords.emplace_back(L"SHADOW_QUALITY_VERYHIGH");
					break;
				}
		}
		switch (UserData::GetInstance().shadowType)
		{
			case ShadowType::Hard:
				{
					enableKeywords.emplace_back(L"HARD_SHADOW");
					break;
				}
			case ShadowType::Soft:
				{
					enableKeywords.emplace_back(L"SOFT_SHADOW");
					break;
				}
		} 
		 
		auto& passData = m_pMaterial->GetPassData(passIndex);
		
		auto emplaceResult = passData.keywords.try_emplace(enableKeywords);
		if(emplaceResult.second)
		{
			auto VariantManager = passData.pShader->GetVariantManager();
			auto currVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);
			auto resourceLayouts = currVariantData->pMeshResourceLayout.get();
			
			passData.pCurrVariantData = currVariantData;
			
			{
				RenderTargetDesc RTDesc = CreateDefaultRenderTargetDesc();
				RTDesc.m_numRenderTargets = 0;
				RTDesc.m_depthStencilFormat = LightManager::GetInstance().GetMainShadowRT()->GetFormat();
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

	void ShadowPass::DrawMesh(UINT passIndex)
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
			m_pMaterial->SetFloat(ShaderIDs::cutoff, meshRenderer.m_CBVObjectParameter->cutoff);
			m_pMaterial->SetFloat(ShaderIDs::opacity, meshRenderer.m_CBVObjectParameter->opacity);
			
			SetSpaceResource(passData, PER_OBJECT_SPACE);
			SetSpaceResource(passData, PER_MATERIAL_SPACE);
 
			auto startIndex = mesh->indexDataOffset / sizeof(UINT16);
			auto startVertex = mesh->vertexDataOffset / GetModelImporter()->GetVertexStride();
			auto indexCount = mesh->indexCount;

			m_pCommand->Draw(indexCount, startVertex, static_cast<UINT>(startIndex));
		}
	}

	void ShadowPass::DrawShadowPass()
	{
		auto pShadowRT = LightManager::GetInstance().GetMainShadowRT();
		m_pCommand->AddBarrier(pShadowRT, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_pCommand->ClearDepthStencilTarget(pShadowRT, 1.f, 0);
		
		if (IsRenderTextureReady({pShadowRT}))
		{
			PipelineInfo pipelineStateData{};
			pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(ShaderPassIDs::ShadowCastPassID).pPipelineStateObject;
			pipelineStateData.m_renderTargets = {};
			pipelineStateData.m_depthStencilTarget = pShadowRT->GetTexture();
			m_pCommand->SetPipeline(pipelineStateData);
			
			m_pCommand->SetViewport(reinterpret_cast<DX12DirectionLight*>(m_pMainLight)->GetMainShadow()->GetViewport());
			m_pCommand->SetScissorRect(reinterpret_cast<DX12DirectionLight*>(m_pMainLight)->GetMainShadow()->GetScissorRect());
			DrawMesh(ShaderPassIDs::ShadowCastPassID);
		}

		m_pCommand->AddBarrier(pShadowRT, D3D12_RESOURCE_STATE_DEPTH_READ);
	}
}
