#include "stdafx.h"
#include "ShadowPass.h"

#include "lib/DX12/DX12Material.h"
#include "RenderResource.h" 
#include "DX12/UploadRingBuffer.h"
#include "lib/Utility/PIXHelper.h"
#include "Manager/PSOManager.h"
#include "lib/Utility/SobolSequenceGenerator.h"
#include "Manager/CameraManager.h"
#include "Manager/RenderTargetManager.h"

namespace ElysiaRenderer
{
	int ShadowPass::ShaderPasseIDs::ShadowCastPassID = -1;
	
	size_t ShadowPass::RenderTextureIDs::ShadowRTID = SIZE_MAX;
	
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
		RenderTextureIDs::ShadowRTID = PropertyToID("Shadow RT");
		
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
	void ShadowPass::Dispose()
	{

	}

	void ShadowPass::Configure()
	{
		m_pMainLight = LightManager::GetInstance().GetMainLight();
		CreateMainShadow(1000, DXGI_FORMAT_D24_UNORM_S8_UINT);
		RenderResource::GetInstance().GetCBVFrameVariable()->ShadowTexIndex = m_pShadowRT->GetTexture()->GetResourceHeapIndex();
		
		m_shaderPasses =
		{ 
			ShaderPass
			{ 
				.Name = "Shadow Cast Pass",
				.FilePath = L"Shaders\\public\\Shadow.hlsl",
			} 
		};
		m_pMaterial = std::move(std::make_unique<Material>(m_pDevice, m_shaderPasses));
		ShaderPasseIDs::ShadowCastPassID = m_pMaterial->FindPassIndex("Shadow Cast Pass");

		m_sobolSqeuences = Create2DSobolSqeuence(64);
		UpdateVariant();
	}
	void ShadowPass::Execute()
	{
		UpdatePSO();
		m_pMainShadow->UpdateShadowTransform(m_pMainLight);
		RenderResource::GetInstance().GetCBVFrameVariable()->ShadowTexIndex = m_pShadowRT->GetTexture()->GetResourceHeapIndex();
		RenderResource::GetInstance().GetCBVFrameVariable()->shadowMatrix = m_pMainShadow->GetShadowMat();
		RenderResource::GetInstance().GetCBVFrameVariable()->shadowSize = GetScreenSize(Vector2(m_pMainShadow->GetWidth(), m_pMainShadow->GetHeight()));

		m_pMaterial->SetFloat(ShaderIDs::shadowNearZ, m_pMainShadow->GetNearZ());
		m_pMaterial->SetFloat(ShaderIDs::shadowFarZ, m_pMainShadow->GetFarZ());
		m_pMaterial->SetFloat(ShaderIDs::shadowDepthBias, UserData::GetInstance().shadowDepthBias / 100);
		m_pMaterial->SetFloat(ShaderIDs::shadowSlopeDepthBias, UserData::GetInstance().shadowSlopeDepthBias / 100);
		m_pMaterial->SetFloat(ShaderIDs::shadowMaxSlopeDepthBias, UserData::GetInstance().shadowMaxSlopeDepthBias / 100);
		m_pMaterial->SetVector2Array(ShaderIDs::g_sobolSequence, m_sobolSqeuences);

		auto pCameraManager = &CameraManager::GetInstance();
		auto passParameter = RenderResource::GetInstance().GetCBVFrameVariable();
		passParameter->cameraPosWS = CameraManager::GetInstance().GetMainCamera()->GetPosition4();
		passParameter->lightData = std::move(LightManager::GetInstance().GetMainLight()->CreateLightData());
		passParameter->frameIndex = m_pDevice->GetFrameIndex();
		passParameter->nearZ = CameraManager::GetInstance().GetMainCamera()->GetNearZ();
		passParameter->farZ = CameraManager::GetInstance().GetMainCamera()->GetFarZ();
		passParameter->ZBufferParams = Vector4(1 - CameraManager::GetInstance().GetMainCamera()->GetFarZ() / pCameraManager->GetMainCamera()->GetNearZ(),
			pCameraManager->GetMainCamera()->GetFarZ() / pCameraManager->GetMainCamera()->GetNearZ(),
			(1 - pCameraManager->GetMainCamera()->GetFarZ() / pCameraManager->GetMainCamera()->GetNearZ()) / pCameraManager->GetMainCamera()->GetFarZ(),
			(pCameraManager->GetMainCamera()->GetFarZ() / pCameraManager->GetMainCamera()->GetNearZ()) / pCameraManager->GetMainCamera()->GetFarZ());
		passParameter->shadowMatrix = m_pMainShadow->GetShadowMat();
		passParameter->OpaqueColorIndex = BufferManager::GetInstance().GetCameraColorRT()->GetTexture()->GetResourceHeapIndex();
		passParameter->OpaqueDepthIndex = BufferManager::GetInstance().GetCameraDepthRT()->GetTexture()->GetResourceHeapIndex();
		passParameter->ShadowTexIndex = m_pShadowRT->GetTexture()->GetResourceHeapIndex();
		
		auto GPUAddress = UploadFrameConstant(m_pDevice->GetGlobalUploadBuffer(), sizeof(CBVFrameVariable),
			RenderResource::GetInstance().GetPerFrameBindResourceSpace()->GetCPUPtr());
		RenderResource::GetInstance().GetPerFrameBindResourceSpace()->SetDynamicCBV(GPUAddress);
		RenderResource::GetInstance().GetPerFrameBindResourceSpace()->Lock();
	} 
	void ShadowPass::Render()
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "Shadow Pass");

		Execute();

		DrawShadowPass();
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
		m_pShadowRT = RenderTargetManager::GetInstance().CreateRenderTexture(
			static_cast<UINT64>(resolution),
			static_cast<UINT64>(resolution),
			format,
			true,
			RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::ShadowRTID));

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
		return m_pShadowRT;
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
			
			passData.pCurrVariantData = currVariantData;
			
			{
				RenderTargetDesc RTDesc = CreateDefaultRenderTargetDesc();
				RTDesc.m_numRenderTargets = 0;
				RTDesc.m_depthStencilFormat = GetShadowRT()->GetFormat();
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
		m_pCommand->AddBarrier(m_pShadowRT, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_pCommand->ClearDepthStencilTarget(m_pShadowRT, 1.f, 0);
		
		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(ShaderPasseIDs::ShadowCastPassID).pPipelineStateObject;
		pipelineStateData.m_renderTargets = {};
		pipelineStateData.m_depthStencilTarget = m_pShadowRT->GetTexture();
		m_pCommand->SetPipeline(pipelineStateData);
		
		if (IsRenderTextureReady({m_pShadowRT}))
		{
			DrawMesh(ShaderPasseIDs::ShadowCastPassID);
		}

		m_pCommand->AddBarrier(m_pShadowRT, D3D12_RESOURCE_STATE_DEPTH_READ);
	}
}
