#include "stdafx.h"
#include "ShadowPass.h"

#include "lib/DX12/DX12Material.h"
#include "RenderResource.h" 
#include "lib/Utility/PIXHelper.h"
#include "Material.h"
#include "Manager/PSOManager.h"
#include "lib/Utility/SobolSequenceGenerator.h"
#include "Manager/ShaderVariantManager.h"

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
		m_pMaterial = std::move(std::make_unique<Material>(m_shaderPasses));
		ShaderPasseIDs::ShadowCastPassID = m_pMaterial->FindPassIndex("Shadow Cast Pass");

		UpdateVariant();
		
		{
			RenderTargetDesc RTDesc = CreateDefaultRenderTargetDesc();
			RTDesc.m_numRenderTargets = 0;
			RTDesc.m_depthStencilFormat = GetShadowRT()->GetFormat();
			auto emplaceResult = m_PipelineStateObjects.try_emplace(ShaderPasseIDs::ShadowCastPassID);
			if (emplaceResult.second)
			{
				emplaceResult.first->second = GetPSOManager()->GetGraphicsPipelineState(m_pMaterial.get(), ShaderPasseIDs::ShadowCastPassID, RTDesc);
			}
		}

		
		
	}
	void ShadowPass::Execute()
	{
		UpdatePSO();
		m_pMainShadow->UpdateShadowTransform(m_pMainLight);
		GetRenderResource()->GetCBVFrameVariable()->ShadowTexIndex = m_pShadowRT->GetTexture()->GetResourceHeapIndex();
		GetRenderResource()->GetCBVFrameVariable()->shadowMatrix = m_pMainShadow->GetShadowMat();
		GetRenderResource()->GetCBVFrameVariable()->shadowSize = GetScreenSize(Vector2(m_pMainShadow->GetWidth(), m_pMainShadow->GetHeight()));

		m_pMaterial->SetConstantVariable(ShaderIDs::shadowNearZ, m_pMainShadow->GetNearZ());
		m_pMaterial->SetConstantVariable(ShaderIDs::shadowFarZ, m_pMainShadow->GetFarZ());
		m_pMaterial->SetConstantVariable(ShaderIDs::shadowDepthBias, UserData::GetInstance().shadowDepthBias / 100);
		m_pMaterial->SetConstantVariable(ShaderIDs::shadowSlopeDepthBias, UserData::GetInstance().shadowSlopeDepthBias / 100);
		m_pMaterial->SetConstantVariable(ShaderIDs::shadowMaxSlopeDepthBias, UserData::GetInstance().shadowMaxSlopeDepthBias / 100);

		auto sobolSequence = Create2DSobolSqeuence(64);
		m_pMaterial->SetConstantVariable(ShaderIDs::g_sobolSequence, sobolSequence);

		m_pMaterial->ApplyConstantData();
	}
	void ShadowPass::Render()
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "Shadow Pass");

		Execute();

		m_pCommand->AddBarrier(m_pShadowRT.get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_pCommand->ClearDepthStencilTarget(m_pShadowRT.get(), 1.f, 0);

		m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pCommand->SetIndexBuffer(GetBufferManager()->GetIndexBufferView()); 
		m_pCommand->SetVertexBuffer(0, 1, const_cast<D3D12_VERTEX_BUFFER_VIEW&>(GetBufferManager()->GetVertexBufferView()));

		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = m_PipelineStateObjects[ShaderPasseIDs::ShadowCastPassID];
		pipelineStateData.m_renderTargets = {};
		pipelineStateData.m_depthStencilTarget = m_pShadowRT->GetTexture();

		bool isReady = true;
		{
			if (m_pShadowRT->GetTexture() == nullptr)
			{
				ThrowRuntimeError("null tex resource");;
			}
			isReady &= m_pShadowRT->GetTexture()->GetIsReady();
		}
		if (isReady)
		{
			m_pCommand->SetViewport(m_pMainShadow->GetViewport());
			m_pCommand->SetScissorRect(m_pMainShadow->GetScissorRect());
			m_pCommand->SetPipeline(pipelineStateData);
			m_pCommand->SetPipelineResource(PER_PASS_SPACE, m_pMaterial->GetPassData(ShaderPasseIDs::ShadowCastPassID).MeshResourceLayouts->m_spaces[PER_PASS_SPACE]);
			m_pCommand->SetPipelineResource(PER_FRAME_SPACE, GetRenderResource()->GetPerFrameBindResourceSpace());

			UINT vertexStride = GetModelImporter()->GetVertexStride();

			for (UINT meshIndex = 0; meshIndex < GetModelImporter()->GetMeshCount(); ++meshIndex)
			{
				const auto& meshRenderer = GetModelImporter()->GetMeshRenderer(meshIndex);
				const auto& mesh = meshRenderer.m_mesh;

				{
					std::unique_ptr<PipelineResourceSpace> pPipelineResourceSpace = std::make_unique<PipelineResourceSpace>();
					pPipelineResourceSpace->SetCBV(meshRenderer.m_objectBuffers[GetDevice()->GetFrameID()].get());
					pPipelineResourceSpace->Lock();
					if (m_pMaterial->GetPassData(ShaderPasseIDs::ShadowCastPassID).MeshResourceLayouts->m_spaces[PER_OBJECT_SPACE] != nullptr)
					{
						delete m_pMaterial->GetPassData(ShaderPasseIDs::ShadowCastPassID).MeshResourceLayouts->m_spaces[PER_OBJECT_SPACE];
						m_pMaterial->GetPassData(ShaderPasseIDs::ShadowCastPassID).MeshResourceLayouts->m_spaces[PER_OBJECT_SPACE] = nullptr;
					}
					m_pMaterial->GetPassData(ShaderPasseIDs::ShadowCastPassID).MeshResourceLayouts->m_spaces[PER_OBJECT_SPACE] = pPipelineResourceSpace.release();
					m_pMaterial->SetConstantVariable(ShaderIDs::worldMatrix, meshRenderer.m_CBVObjectParameter->worldMatrix);
					m_pMaterial->ApplyConstantData();
					m_pCommand->SetPipelineResource(PER_OBJECT_SPACE, m_pMaterial->GetPassData(ShaderPasseIDs::ShadowCastPassID).MeshResourceLayouts->m_spaces[PER_OBJECT_SPACE]);
				}

				{
					std::unique_ptr<PipelineResourceSpace> pPipelineResourceSpace = std::make_unique<PipelineResourceSpace>();
					pPipelineResourceSpace->SetCBV(meshRenderer.m_materialBuffers[GetDevice()->GetFrameID()].get());
					pPipelineResourceSpace->Lock();
					if (m_pMaterial->GetPassData(ShaderPasseIDs::ShadowCastPassID).MeshResourceLayouts->m_spaces[PER_MATERIAL_SPACE] != nullptr)
					{
						delete m_pMaterial->GetPassData(ShaderPasseIDs::ShadowCastPassID).MeshResourceLayouts->m_spaces[PER_MATERIAL_SPACE];
						m_pMaterial->GetPassData(ShaderPasseIDs::ShadowCastPassID).MeshResourceLayouts->m_spaces[PER_MATERIAL_SPACE] = nullptr;
					}
					m_pMaterial->GetPassData(ShaderPasseIDs::ShadowCastPassID).MeshResourceLayouts->m_spaces[PER_MATERIAL_SPACE] = pPipelineResourceSpace.release();

					m_pMaterial->SetConstantVariable(ShaderIDs::baseColorTexIndex, meshRenderer.m_CBVObjectParameter->baseColorTexIndex);
					m_pMaterial->SetConstantVariable(ShaderIDs::opacity, meshRenderer.m_CBVObjectParameter->opacity);
					m_pMaterial->SetConstantVariable(ShaderIDs::cutoff, meshRenderer.m_CBVObjectParameter->cutoff);
					m_pMaterial->ApplyConstantData();
					m_pCommand->SetPipelineResource(PER_MATERIAL_SPACE, m_pMaterial->GetPassData(ShaderPasseIDs::ShadowCastPassID).MeshResourceLayouts->m_spaces[PER_MATERIAL_SPACE]);
				}

				auto startIndex = mesh->indexDataOffset / sizeof(UINT16);
				auto startVertex = mesh->vertexDataOffset / vertexStride;
				auto VertexCount = mesh->vertexCount;
				auto indexCount = mesh->indexCount;

				m_pCommand->Draw(indexCount, startVertex, static_cast<UINT>(startIndex));
			}
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
	
	ShaderVariantData ShadowPass::UpdateVariant()
	{
		switch (UserData::GetInstance().shadowQuality)
		{
			case ShadowQuality::Low:
			{
				m_enableKeywords.emplace_back(L"SHADOW_QUALITY_LOW");
				break;
			}
			case ShadowQuality::Middle:
			{
				m_enableKeywords.emplace_back(L"SHADOW_QUALITY_MIDDLE");
				break;
			}
			case ShadowQuality::High:
			{
				m_enableKeywords.emplace_back(L"SHADOW_QUALITY_HIGH");
				break;
			}
			case ShadowQuality::VeryHigh:
			{
				m_enableKeywords.emplace_back(L"SHADOW_QUALITY_VERYHIGH");
				break;
			}
		}
		switch (UserData::GetInstance().shadowType)
		{
			case ShadowType::Hard:
			{
				m_enableKeywords.emplace_back(L"HARD_SHADOW");
				break;
			}
			case ShadowType::Soft:
			{
				m_enableKeywords.emplace_back(L"SOFT_SHADOW");
				break;
			}
		}
		
		auto VariantManager = m_pMaterial->GetPassData(ShaderPasseIDs::ShadowCastPassID).pShader->GetVariantManager();
		auto& currVariantData = VariantManager->GetOrCompileVariantByNames(m_enableKeywords);

		if (m_pMaterial->HasMeshRender())
		{
			for (UINT meshIndex = 0; meshIndex < GetModelImporter()->GetMeshCount(); ++meshIndex)
			{
				auto& meshRenderer = GetModelImporter()->GetMeshRenderer(meshIndex);

				for (UINT frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; ++frameIndex)
				{
					auto objectBufferDesc = currVariantData.MeshResourceLayouts->m_spaces[PER_OBJECT_SPACE]->GetCBVDesc();
					if (meshRenderer.m_objectBuffers[frameIndex] &&
						meshRenderer.m_objectBuffers[frameIndex]->GetResourceDesc().Width != objectBufferDesc.m_size)
					{
						meshRenderer.m_objectBuffers[frameIndex].reset();
						meshRenderer.m_objectBuffers[frameIndex] = std::move(GetDevice()->CreateBuffer(objectBufferDesc));
					}

					auto materialBufferDesc = currVariantData.MeshResourceLayouts->m_spaces[PER_MATERIAL_SPACE]->GetCBVDesc();
					if (meshRenderer.m_materialBuffers[frameIndex] &&
						meshRenderer.m_materialBuffers[frameIndex]->GetResourceDesc().Width != materialBufferDesc.m_size)
					{
						meshRenderer.m_materialBuffers[frameIndex].reset();
						meshRenderer.m_materialBuffers[frameIndex] = std::move(GetDevice()->CreateBuffer(materialBufferDesc));
					}
				}
			}
		}
		
	}

}
