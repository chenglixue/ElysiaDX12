#include "stdafx.h"
#include "ShadowPass.h"

#include "DX12Material.h"
#include "RenderResource.h"
#include "PIXHelper.h"
#include "RenderMaterial.h"
#include "PSOManager.h"
#include "SobolSequenceGenerator.h"

namespace ElysiaRenderer
{
	ShadowPass::ShadowPass(DX12Camera* pCamera) :
		BasePass(pCamera)
	{

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

		m_shaderPasses.emplace_back(ShaderPass
			{
				.Name = "Shadow Cast Pass",
				.FilePath = L"Shaders\\public\\Shadow.hlsl",
				.VertexEntryPoint = L"VS",
				.FragmentEntryPoint = L"PS",
				.RasterizerDesc = GetRasterizerState(RasterizerState::BackFaceCull),
				.BlendDesc = GetBlendState(BlendState::Disabled),
				.DepthStencilDesc = GetDepthState(DepthState::WritesEnabled)
			});
		m_pMaterial = std::move(std::make_unique<RenderMaterial>(m_shaderPasses));
		ShaderPasseIDs::ShadowCast = m_pMaterial->FindPassIndex("Shadow Cast Pass");

		{
			RenderTargetDesc RTDesc = CreateDefaultRenderTargetDesc();
			RTDesc.m_numRenderTargets = 0;
			RTDesc.m_depthStencilFormat = GetShadowRT()->GetFormat();
			auto emplaceResult = m_PipelineStateObjects.try_emplace(ShaderPasseIDs::ShadowCast);
			if (emplaceResult.second)
			{
				emplaceResult.first->second = GetPSOManager()->GetGraphicsPipelineState(m_pMaterial.get(), ShaderPasseIDs::ShadowCast, RTDesc);
			}
		}
	}
	void ShadowPass::Execute()
	{
		m_pMainShadow->UpdateShadowTransform(m_pMainLight);
		GetRenderResource()->GetCBVFrameVariable()->ShadowTexIndex = m_pShadowRT->GetFormat();
		GetRenderResource()->GetCBVFrameVariable()->shadowMatrix = m_pMainShadow->GetShadowMat();
		GetRenderResource()->GetCBVFrameVariable()->shadowSize = GetScreenSize(Vector2(m_pMainShadow->GetWidth(), m_pMainShadow->GetHeight()));

		m_pMaterial->SetConstantVariable<float>("shadowNearZ", m_pMainShadow->GetNearZ());
		m_pMaterial->SetConstantVariable<float>("shadowFarZ", m_pMainShadow->GetFarZ());
		m_pMaterial->SetConstantVariable<float>("shadowDepthBias", UserData::GetInstance().shadowDepthBias / 100);
		m_pMaterial->SetConstantVariable<float>("shadowSlopeDepthBias", UserData::GetInstance().shadowSlopeDepthBias / 100);
		m_pMaterial->SetConstantVariable<float>("shadowMaxSlopeDepthBias", UserData::GetInstance().shadowMaxSlopeDepthBias / 100);

		auto sobolSequence = Create2DSobolSqeuence(64);
		m_pMaterial->SetConstantVariable<std::vector<Vector2>>("g_sobolSequence", sobolSequence);

		m_pMaterial->ApplyConstantData();
	}
	void ShadowPass::Render()
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "Shadow Pass");

		Execute();

		m_pCommand->AddBarrier(*m_pShadowRT->GetTexture(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_pCommand->FlushBarrier(); 

		m_pCommand->ClearDepthStencilTarget(*m_pShadowRT, 1.f, 0);

		m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pCommand->SetIndexBuffer(GetBufferManager()->GetIndexBufferView());
		m_pCommand->SetVertexBuffer(0, 1, const_cast<D3D12_VERTEX_BUFFER_VIEW&>(GetBufferManager()->GetVertexBufferView()));

		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = m_PipelineStateObjects[ShaderPasseIDs::ShadowCast];
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
			m_pCommand->SetPipelineResource(PER_PASS_SPACE, m_pMaterial->GetPassData(ShaderPasseIDs::ShadowCast).MeshResourceLayouts->m_spaces[PER_PASS_SPACE]);

			UINT vertexStride = GetModelImporter()->GetVertexStride();

			for (UINT meshIndex = 0; meshIndex < GetModelImporter()->GetMeshCount(); ++meshIndex)
			{
				const auto& meshRenderer = GetModelImporter()->GetMeshRenderer(meshIndex);
				const auto& mesh = meshRenderer.m_mesh;

				{
					m_pMaterial->SetConstantVariable<Matrix>("worldMatrix", meshRenderer.m_CBVObjectParameter->worldMatrix);
					m_pMaterial->ApplyConstantData();
					m_pCommand->SetPipelineResource(PER_OBJECT_SPACE, m_pMaterial->GetPassData(ShaderPasseIDs::ShadowCast).MeshResourceLayouts->m_spaces[PER_OBJECT_SPACE]);
				}

				{
					m_pMaterial->SetConstantVariable<UINT>("baseColorTexIndex", meshRenderer.m_CBVObjectParameter->baseColorTexIndex);
					m_pMaterial->SetConstantVariable<float>("opacity", meshRenderer.m_CBVObjectParameter->opacity);
					m_pMaterial->SetConstantVariable<float>("cutoff", meshRenderer.m_CBVObjectParameter->cutoff);
					m_pMaterial->ApplyConstantData();
					m_pCommand->SetPipelineResource(PER_MATERIAL_SPACE, m_pMaterial->GetPassData(ShaderPasseIDs::ShadowCast).MeshResourceLayouts->m_spaces[PER_MATERIAL_SPACE]);
				}

				auto startIndex = mesh->indexDataOffset / sizeof(UINT16);
				auto startVertex = mesh->vertexDataOffset / vertexStride;
				auto VertexCount = mesh->vertexCount;
				auto indexCount = mesh->indexCount;

				m_pCommand->Draw(indexCount, startVertex, startIndex);
			}
		}

		m_pCommand->AddBarrier(*m_pShadowRT->GetTexture(), D3D12_RESOURCE_STATE_DEPTH_READ);
		m_pCommand->FlushBarrier();
	}

	void ShadowPass::Dispose()
	{

	}

	void ShadowPass::SetupShaderData()
	{
		shaderPasses.emplace_back(ShaderPass
			{
				.Name = "Shadow Cast Pass",
				.FilePath = L"Shaders\\public\\Shadow.hlsl",
				.VertexEntryPoint = L"VS",
				.FragmentEntryPoint = L"PS",
				.RasterizerDesc = GetRasterizerState(RasterizerState::BackFaceCull),
				.BlendDesc = GetBlendState(BlendState::Disabled),
				.DepthStencilDesc = GetDepthState(DepthState::WritesEnabled)
			});

		
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
}