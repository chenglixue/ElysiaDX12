#include "stdafx.h"
#include "ShadowPass.h"

#include "lib/DX12/DX12Material.h"
#include "RenderResource.h" 
#include "lib/Utility/PIXHelper.h"
#include "Manager/PSOManager.h"
#include "lib/Utility/SobolSequenceGenerator.h"

namespace ElysiaRenderer
{
	int ShadowPass::ShaderPasseIDs::ShadowCastPassID = -1;
	size_t ShadowPass::ShaderIDs::shadowNearZ = SIZE_MAX;
	size_t ShadowPass::ShaderIDs::shadowFarZ = SIZE_MAX;
	size_t ShadowPass::ShaderIDs::shadowDepthBias = SIZE_MAX;
	size_t ShadowPass::ShaderIDs::shadowSlopeDepthBias = SIZE_MAX;
	size_t ShadowPass::ShaderIDs::shadowMaxSlopeDepthBias = SIZE_MAX;
	size_t ShadowPass::ShaderIDs::g_sobolSequence = SIZE_MAX;
	size_t ShadowPass::ShaderIDs::worldMatrix = SIZE_MAX;
	size_t ShadowPass::ShaderIDs::baseColorTexIndex = SIZE_MAX;
	size_t ShadowPass::ShaderIDs::opacity = SIZE_MAX;
	size_t ShadowPass::ShaderIDs::cutoff = SIZE_MAX;

	ShadowPass::ShadowPass(DX12Camera* pCamera) :
		BasePass(pCamera)
	{
		ShaderIDs::shadowNearZ = PropertyToID("shadowNearZ");
		ShaderIDs::shadowFarZ = PropertyToID("shadowFarZ");
		ShaderIDs::shadowDepthBias = PropertyToID("shadowDepthBias");
		ShaderIDs::shadowSlopeDepthBias = PropertyToID("shadowSlopeDepthBias");
		ShaderIDs::shadowMaxSlopeDepthBias = PropertyToID("shadowMaxSlopeDepthBias");
		ShaderIDs::g_sobolSequence = PropertyToID("g_sobolSequence");
		ShaderIDs::worldMatrix = PropertyToID("worldMatrix");
		ShaderIDs::baseColorTexIndex = PropertyToID("baseColorTexIndex");
		ShaderIDs::opacity = PropertyToID("opacity");
		ShaderIDs::cutoff = PropertyToID("cutoff");
	};
	ShadowPass::~ShadowPass()
	{
		Dispose();
	}

	void ShadowPass::Configure()
	{
		m_pMainLight = GetLightManager()->GetMainLight();
		CreateMainShadow(1000, DXGI_FORMAT_D24_UNORM_S8_UINT);
		GetRenderResource()->GetCBVFrameVariable()->ShadowTexIndex = m_pShadowRT->GetTexture()->GetResourceHeapIndex();
		
		m_shaderPasses = std::vector<ShaderPass>
		{ 
			ShaderPass
			{ 
				.Name = "Shadow Cast Pass",
				.FilePath = L"Shaders\\public\\Shadow.hlsl",
			} 
		}; 
		m_pMaterial = std::move(std::make_unique<Material>(m_shaderPasses, &GetModelImporter()->GetMeshRenderer(0)));
		ShaderPasseIDs::ShadowCastPassID = m_pMaterial->FindPassIndex("Shadow Cast Pass");

		UpdateVariant();
	}
	void ShadowPass::Execute()
	{
		UpdatePSO();
		m_pMainShadow->UpdateShadowTransform(m_pMainLight);
		GetRenderResource()->GetCBVFrameVariable()->ShadowTexIndex = m_pShadowRT->GetTexture()->GetResourceHeapIndex();
		GetRenderResource()->GetCBVFrameVariable()->shadowMatrix = m_pMainShadow->GetShadowMat();
		GetRenderResource()->GetCBVFrameVariable()->shadowSize = GetScreenSize(Vector2(m_pMainShadow->GetWidth(), m_pMainShadow->GetHeight()));

		m_pMaterial->SetFloat(ShaderIDs::shadowNearZ, m_pMainShadow->GetNearZ());
		m_pMaterial->SetFloat(ShaderIDs::shadowFarZ, m_pMainShadow->GetFarZ());
		m_pMaterial->SetFloat(ShaderIDs::shadowDepthBias, UserData::GetInstance().shadowDepthBias / 100);
		m_pMaterial->SetFloat(ShaderIDs::shadowSlopeDepthBias, UserData::GetInstance().shadowSlopeDepthBias / 100);
		m_pMaterial->SetFloat(ShaderIDs::shadowMaxSlopeDepthBias, UserData::GetInstance().shadowMaxSlopeDepthBias / 100);
		m_pMaterial->SetVector2Array(ShaderIDs::g_sobolSequence, Create2DSobolSqeuence(64));
	} 
	void ShadowPass::Render()
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "Shadow Pass");

		Execute();

		m_pCommand->AddBarrier(m_pShadowRT.get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_pCommand->ClearDepthStencilTarget(m_pShadowRT.get(), 1.f, 0);
		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(ShaderPasseIDs::ShadowCastPassID).pPipelineStateObject;
		pipelineStateData.m_renderTargets = {};
		pipelineStateData.m_depthStencilTarget = m_pShadowRT->GetTexture();
		m_pCommand->SetPipeline(pipelineStateData);
		
		if (IsTexReady({m_pShadowRT.get()}))
		{
			DrawMesh(ShaderPasseIDs::ShadowCastPassID);
		}

		m_pCommand->AddBarrier(m_pShadowRT.get(), D3D12_RESOURCE_STATE_DEPTH_READ);
	}
	
	void ShadowPass::Dispose()
	{

	}
	
	void ShadowPass::CreateMainShadow(float boundSphereRadius, DXGI_FORMAT format)
	{
		float resolution;
		switch (UserData::GetInstance().shadowQuality)
		{
		case ShadowQuality::Low:
		{
			resolution = 512;
			break;
		}
		case ShadowQuality::Middle:
		{
			resolution = 1024;
			break;
		}
		case ShadowQuality::High:
		{
			resolution = 2048;
			break;
		}
		case ShadowQuality::VeryHigh:
		{
			resolution = 4096;
			break;
		}
		default:
		{
			resolution = 1024;
			ThrowRuntimeError("inivalid shadow quality");
			break;
		}
		}
		m_pShadowRT = CreateRenderTexture(
			static_cast<UINT64>(resolution),
			static_cast<UINT64>(resolution),
			format,
			true,
			L"Shadow RT");

		auto shadowMap = std::make_unique<DX12Shadow>(m_pShadowRT->GetTexture());
		shadowMap->InitBoundSphere(boundSphereRadius);

		if (m_pMainShadow != nullptr)
		{
			m_pMainShadow.reset();
			m_pMainShadow = std::move(shadowMap);
		}
		else
		{
			m_pMainShadow = std::move(shadowMap);
		}
	}
	RenderTexture* ShadowPass::GetShadowRT() const
	{
		return m_pShadowRT.get();
	}
	
	void ShadowPass::UpdatePSO()
	{
		
	}
	void ShadowPass::UpdateVariant()
	{
		UpdateShadowPassVariant(ShaderPasseIDs::ShadowCastPassID);
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
			
			if(passData.pCurrVariantData == nullptr || passData.pCurrVariantData != currVariantData)
			{
				m_pMaterial->SetPipelineResourceLayout(resourceLayouts);
				passData.pCurrVariantData = currVariantData;

				passData.pPassGPUPtr = resourceLayouts->m_spaces[PER_PASS_SPACE]->GetCBV()->GetMappedBuffer();
				passData.pFrameGPUPtr = resourceLayouts->m_spaces[PER_FRAME_SPACE]->GetCBV()->GetMappedBuffer();
			}
			
			{
				RenderTargetDesc RTDesc = CreateDefaultRenderTargetDesc();
				RTDesc.m_numRenderTargets = 0;
				RTDesc.m_depthStencilFormat = GetShadowRT()->GetFormat();
				
				passData.pPipelineStateObject = GetPSOManager()->GetGraphicsPipelineState(m_pMaterial.get(), ShaderPasseIDs::ShadowCastPassID, RTDesc);

				emplaceResult.first->second = 
				{
					.pCurrVariantData = currVariantData,
					.pPipelineStateObject = passData.pPipelineStateObject,
					.pPassGPUPtr = resourceLayouts->m_spaces[PER_PASS_SPACE]->GetCBV()->GetMappedBuffer(),
					.pFrameGPUPtr = resourceLayouts->m_spaces[PER_FRAME_SPACE]->GetCBV()->GetMappedBuffer(),
				};
			}
		}
		else
		{
			const auto& saveData = passData.keywords.at(enableKeywords);
			
			passData.pCurrVariantData = saveData.pCurrVariantData;
			passData.pPipelineStateObject = saveData.pPipelineStateObject;
			passData.pPassGPUPtr = saveData.pPassGPUPtr;
			passData.pFrameGPUPtr = saveData.pFrameGPUPtr;
		}

		m_pMaterial->CreateMaterialCBuffer(passIndex);
		m_pMaterial->SetMaterialCBufferGPUPtr(PER_PASS_SPACE, passIndex);
		m_pMaterial->SetMaterialCBufferGPUPtr(PER_FRAME_SPACE, passIndex);
	}

	void ShadowPass::SetObjectResource(const MeshRender& meshRender, PipelineResourceLayout* pResourceLayout, UINT passIndex)
	{
		assert(pResourceLayout);
		assert(meshRender.m_mesh);
		
		if (pResourceLayout->m_spaces[PER_OBJECT_SPACE] != nullptr)
		{
			delete pResourceLayout->m_spaces[PER_OBJECT_SPACE];
			pResourceLayout->m_spaces[PER_OBJECT_SPACE] = nullptr;
		}
		auto newObjectSpace = std::make_unique<PipelineResourceSpace>();
		newObjectSpace->SetCBV(meshRender.m_objectBuffers[GetDevice()->GetFrameID()].get());
		newObjectSpace->Lock();
		pResourceLayout->m_spaces[PER_OBJECT_SPACE] = newObjectSpace.release();
		m_pMaterial->SetMaterialCBufferGPUPtr(PER_OBJECT_SPACE, passIndex);
						
		m_pCommand->SetPipelineResource(PER_OBJECT_SPACE, pResourceLayout->m_spaces[PER_OBJECT_SPACE]);
	}
	void ShadowPass::SetMaterialResource(const MeshRender& meshRender, PipelineResourceLayout* pResourceLayout, UINT passIndex)
	{
		assert(pResourceLayout);
		assert(meshRender.m_mesh);
		
		if (pResourceLayout->m_spaces[PER_MATERIAL_SPACE] != nullptr)
		{
			delete pResourceLayout->m_spaces[PER_MATERIAL_SPACE];
			pResourceLayout->m_spaces[PER_MATERIAL_SPACE] = nullptr;
		}
		auto newSpace = std::make_unique<PipelineResourceSpace>();
		newSpace->SetCBV(meshRender.m_materialBuffers[GetDevice()->GetFrameID()].get());
		newSpace->Lock();
		pResourceLayout->m_spaces[PER_MATERIAL_SPACE] = newSpace.release();
		m_pMaterial->SetMaterialCBufferGPUPtr(PER_MATERIAL_SPACE, passIndex);

		m_pCommand->SetPipelineResource(PER_MATERIAL_SPACE, pResourceLayout->m_spaces[PER_MATERIAL_SPACE]);
	}
	void ShadowPass::DrawMesh(UINT passIndex)
	{
		m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pCommand->SetIndexBuffer(GetBufferManager()->GetIndexBufferView()); 
		m_pCommand->SetVertexBuffer(0, 1, const_cast<D3D12_VERTEX_BUFFER_VIEW&>(GetBufferManager()->GetVertexBufferView()));
		
		auto& passData = m_pMaterial->GetPassData(passIndex);
		auto meshResourceLayout = passData.GetMeshResourceLayout();
		
		if(meshResourceLayout->IsValidSpace(PER_PASS_SPACE))
		{
			m_pCommand->SetPipelineResource(PER_PASS_SPACE, meshResourceLayout->m_spaces[PER_PASS_SPACE]);
		}
		if(meshResourceLayout->IsValidSpace(PER_FRAME_SPACE))
		{
			m_pCommand->SetPipelineResource(PER_FRAME_SPACE, GetRenderResource()->GetPerFrameBindResourceSpace());
		}
		
		for (UINT meshIndex = 0; meshIndex < GetModelImporter()->GetMeshCount(); ++meshIndex)
		{
			const auto& meshRenderer = GetModelImporter()->GetMeshRenderer(meshIndex);
			const auto& mesh = meshRenderer.m_mesh;
			
			SetObjectResource(meshRenderer, meshResourceLayout, passIndex);
			SetMaterialResource(meshRenderer, meshResourceLayout, passIndex);

			m_pMaterial->SetMatrix(ShaderIDs::worldMatrix, meshRenderer.m_CBVObjectParameter->worldMatrix);
			m_pMaterial->SetUINT(ShaderIDs::baseColorTexIndex, meshRenderer.m_CBVObjectParameter->baseColorTexIndex);
			m_pMaterial->SetFloat(ShaderIDs::opacity, meshRenderer.m_CBVObjectParameter->opacity + 1);
			m_pMaterial->SetFloat(ShaderIDs::cutoff, meshRenderer.m_CBVObjectParameter->cutoff + 1);
			m_pMaterial->SetFloat(PropertyToID("test"), 1);
			m_pMaterial->Flush();

			auto startIndex = mesh->indexDataOffset / sizeof(UINT16);
			auto startVertex = mesh->vertexDataOffset / GetModelImporter()->GetVertexStride();
			auto indexCount = mesh->indexCount;

			m_pCommand->Draw(indexCount, startVertex, static_cast<UINT>(startIndex));
		}
	}
}
