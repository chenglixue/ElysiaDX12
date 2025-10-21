#include "ShadowPass.h"
#include "Renderer.h"
#include "RenderResource.h"

namespace ElysiaRenderer
{
	std::unique_ptr<ShadowPass> ShadowPass::m_instance;
	std::once_flag ShadowPass::m_initInstanceFlag;

	ShadowPass::~ShadowPass()
	{
		Dispose();
	}

	void ShadowPass::Configure()
	{
		m_pMainLight = GetMainLight();
		CreateMainShadow(15, DXGI_FORMAT_D24_UNORM_S8_UINT);
		RenderResource::GetInstance().GetCBVPassParameter()->ShadowTexIndex = m_pShadowRT->GetSRVIndex();
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
	}
	void ShadowPass::Render()
	{
		Execute();

		m_pCommand->AddBarrier(*m_pShadowRT->GetTexture(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_pCommand->FlushBarrier();

		m_pCommand->ClearDepthStencilTarget(*m_pShadowRT->GetTexture(), 1.f, 0);

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

			UINT vertexStride = m_pModelImporter->GetVertexStride();

			for (UINT meshIndex = 0; meshIndex < m_pModelImporter->GetMeshCount(); ++meshIndex)
			{
				const auto& meshRenderer = m_pModelImporter->GetMeshRenderer(meshIndex);
				const auto& mesh = meshRenderer.m_mesh;

				auto objectContantBuffer = m_pBufferManager->GetMutilConstantBuffer(PER_OBJECT_SPACE, m_device->GetFrameID(), meshIndex);
				m_perObjectBindResourceSpace->SetCBV(objectContantBuffer);
				m_pCommand->SetPipelineResource(PER_OBJECT_SPACE, m_perObjectBindResourceSpace.get());

				auto startIndex = mesh->indexDataOffset / sizeof(UINT16);
				auto startVertex = mesh->vertexDataOffset / vertexStride;
				auto VertexCount = mesh->vertexCount;
				auto indexCount = mesh->indexCount;

				m_pCommand->Draw(indexCount, startVertex, startIndex);
			}
		}

		m_pCommand->AddBarrier(*m_pShadowRT->GetTexture(), D3D12_RESOURCE_STATE_GENERIC_READ);
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

		m_pShadowRT = std::make_unique<RenderTexture>();
		m_pShadowRT->Init(shadowRTDesc);

		auto shadowMap = std::make_unique<DX12Shadow>(m_pShadowRT->GetResource());
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