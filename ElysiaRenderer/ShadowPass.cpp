#include "stdafx.h"
#include "ShadowPass.h"

#include "DX12Material.h"
#include "RenderResource.h"
#include "PIXHelper.h"
#include "RenderMaterial.h"

namespace ElysiaRenderer
{
	ShadowPass::~ShadowPass()
	{
		Dispose();
	}

	void ShadowPass::Configure()
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

		m_pMaterial = std::move(std::make_unique<RenderMaterial>(shaderPasses));

		PipelineStateCreateDesc pipelineStateCreateDesc{};
		pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
		pipelineStateCreateDesc.m_vertexShader = m_pShadowVS.get();
		pipelineStateCreateDesc.m_pixelShader = m_pShadowPS.release();
		pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = 0;
		pipelineStateCreateDesc.m_renderTargetDesc.m_depthStencilFormat = GetShadowRT()->GetFormat();
		pipelineStateCreateDesc.m_rasterDesc = GetRasterizerState(RasterizerState::BackFaceCull);
		pipelineStateCreateDesc.m_blendDesc = GetBlendState(BlendState::Disabled);
		pipelineStateCreateDesc.m_depthStencilDesc = GetDepthState(DepthState::WritesEnabled);
		pipelineStateCreateDesc.m_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		m_pGraphicsPipelineStates[ShaderQueue::Shadow] = std::move(GetDevice()->CreateGraphicsPipelineState(pipelineStateCreateDesc, m_meshResourceLayout));

		m_pMainLight = GetLightManager()->GetMainLight();
		CreateMainShadow(1000, DXGI_FORMAT_D24_UNORM_S8_UINT);
		GetRenderResource()->GetCBVPassParameter()->ShadowTexIndex = m_pShadowRT->GetTexture()->GetResourceHeapIndex();
	}
	void ShadowPass::Execute()
	{
		m_pMainShadow->UpdateShadowTransform(m_pMainLight);

		auto& pUserData = UserData::GetInstance();

		auto shadowDepthBias = pUserData.shadowDepthBias / 100;
		auto shadowSlopeDepthBias = pUserData.shadowSlopeDepthBias / 100;
		auto shadowMaxSlopeDepthBias = pUserData.shadowMaxSlopeDepthBias / 100;

		SetConstantData("shadowMatrix", &m_pMainShadow->GetShadowMat());
		SetConstantData("shadowSize", &GetScreenSize(Vector2(m_pMainShadow->GetWidth(), m_pMainShadow->GetHeight())));
		SetConstantData("shadowNearZ", &m_pMainShadow->GetNearZ());
		SetConstantData("shadowFarZ", &m_pMainShadow->GetFarZ());
		SetConstantData("shadowDepthBias", &shadowDepthBias);
		SetConstantData("shadowSlopeDepthBias", &shadowSlopeDepthBias);
		SetConstantData("shadowMaxSlopeDepthBias", &shadowMaxSlopeDepthBias);

		ApplyConstantData();
	}
	void ShadowPass::Render()
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "Shadow Pass");

		Execute();

		m_pCommand->AddBarrier(*m_pShadowRT->GetTexture(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_pCommand->FlushBarrier();

		m_pCommand->ClearDepthStencilTarget(*m_pShadowRT, 1.f, 0);

		m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pCommand->SetViewport(m_pMainShadow->GetViewport());
		m_pCommand->SetScissorRect(m_pMainShadow->GetScissorRect());
		m_pCommand->SetIndexBuffer(GetBufferManager()->GetIndexBufferView());
		m_pCommand->SetVertexBuffer(0, 1, const_cast<D3D12_VERTEX_BUFFER_VIEW&>(GetBufferManager()->GetVertexBufferView()));

		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = m_pGraphicsPipelineStates[ShaderQueue::Shadow].get();
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