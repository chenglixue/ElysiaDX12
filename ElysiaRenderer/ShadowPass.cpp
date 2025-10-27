#include "ShadowPass.h"
#include "RenderResource.h"

namespace ElysiaRenderer
{
	ShadowPass::~ShadowPass()
	{
		Dispose();
	}

	void ShadowPass::Configure()
	{
		AddShader(ShaderQueue::Shadow, L"Shaders\\public\\Shadow.hlsl", L"VS", ShaderType::Vertex);
		AddShader(ShaderQueue::Shadow, L"Shaders\\public\\Shadow.hlsl", L"PS", ShaderType::Pixel);

		m_pMainLight = GetMainLight();
		CreateMainShadow(1000, DXGI_FORMAT_D24_UNORM_S8_UINT);
		RenderResource::GetInstance().GetCBVPassParameter()->ShadowTexIndex = m_pShadowRT->GetTexture()->GetResourceHeapIndex();

		PipelineStateCreateDesc pipelineStateCreateDesc{};
		PipelineResourceLayout meshResourceLayout{};

		meshResourceLayout.m_spaces[PER_OBJECT_SPACE] = RenderResource::GetPerObjectBindResourceSpace();
		meshResourceLayout.m_spaces[PER_PASS_SPACE] = RenderResource::GetPerMainBindResourceSpace();

		pipelineStateCreateDesc = std::move(CreateDefaultPipelineStateCreateDesc());
		pipelineStateCreateDesc.m_vertexShader = GetVertexShaders()[ShaderQueue::Shadow][ShaderType::Vertex].get();
		pipelineStateCreateDesc.m_pixelShader = GetPixelShaders()[ShaderQueue::Shadow][ShaderType::Pixel].get();
		pipelineStateCreateDesc.m_inputElementDesc = g_inputElementDescs;
		pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets = 0;
		pipelineStateCreateDesc.m_renderTargetDesc.m_depthStencilFormat = GetShadowRT()->GetFormat();
		pipelineStateCreateDesc.m_rasterDesc = GetRasterizerState(RasterizerState::BackFaceCull);
		pipelineStateCreateDesc.m_blendDesc = GetBlendState(BlendState::Disabled);
		pipelineStateCreateDesc.m_depthStencilDesc = GetDepthState(DepthState::WritesEnabled);
		pipelineStateCreateDesc.m_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		(*m_pGraphicsPipelineStates)[ShaderQueue::Shadow] = std::move(GetDevice()->CreateGraphicsPipelineState(pipelineStateCreateDesc, meshResourceLayout));
	}
	void ShadowPass::Execute()
	{
		m_pMainShadow->UpdateShadowTransform(m_pMainLight);

		auto& pUserData = UserData::GetInstance();
		auto passParameter = RenderResource::GetInstance().GetCBVPassParameter();

		passParameter->shadowMatrix = m_pMainShadow->GetShadowMat();
		passParameter->shadowSize = GetScreenSize(Vector2(m_pMainShadow->GetWidth(), m_pMainShadow->GetHeight()));
		passParameter->shadowNearZ = m_pMainShadow->GetNearZ();
		passParameter->shadowFarZ = m_pMainShadow->GetFarZ();

		passParameter->shadowDepthBias = pUserData.shadowDepthBias / 100;
		passParameter->shadowSlopeDepthBias = pUserData.shadowSlopeDepthBias / 100;
		passParameter->shadowMaxSlopeDepthBias = pUserData.shadowMaxSlopeDepthBias / 100;

		GetBufferManager()->GetSingleConstantBuffer(PER_PASS_SPACE)->SetMappedData(passParameter, sizeof(CBVMainPassParameter));
	}
	void ShadowPass::Render()
	{
		Execute();

		m_pCommand->AddBarrier(*m_pShadowRT->GetTexture(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_pCommand->FlushBarrier();

		m_pCommand->ClearDepthStencilTarget(*m_pShadowRT, 1.f, 0);

		m_pCommand->SetViewport(m_pMainShadow->GetViewport());
		m_pCommand->SetScissorRect(m_pMainShadow->GetScissorRect());

		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = (*m_pGraphicsPipelineStates)[ShaderQueue::Shadow].get();
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

	void ShadowPass::CreateMainShadow(float boundSphereRadius, DXGI_FORMAT format)
	{
		RenderTextureDesc shadowRTDesc{};
		shadowRTDesc.Name = L"Shadowm RT";
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
		shadowRTDesc.Width = static_cast<UINT64>(resolution);
		shadowRTDesc.Height = static_cast<UINT64>(resolution);
		shadowRTDesc.Format = format;
		shadowRTDesc.ArraySize = 1;
		shadowRTDesc.Dimension = TextureDimension::Tex2D;
		shadowRTDesc.EnableRandomWrite = false;
		shadowRTDesc.MipmapLevels = 1;
		shadowRTDesc.MSAASamples = 1;
		shadowRTDesc.IsDepth = true;

		m_pShadowRT = std::make_unique<RenderTexture>();
		m_pShadowRT->Init(shadowRTDesc);

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