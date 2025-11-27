#include "stdafx.h"
#include "AOPass.h"

#include "lib/DX12/DX12Device.h"
#include "lib/Utility/RenderTexture.h"
#include "RenderResource.h"

namespace ElysiaRenderer
{
	using namespace ElysiaHelper;

	int AOPass::ShaderPasseIDs::AOPassID = -1;
	int AOPass::ShaderPasseIDs::BlitPassID = -1;

	size_t AOPass::ShaderIDs::g_ScreenSize = SIZE_MAX;
	size_t AOPass::ShaderIDs::viewMatrix = SIZE_MAX;
	size_t AOPass::ShaderIDs::viewMatrix_I = SIZE_MAX;
	size_t AOPass::ShaderIDs::projMatrix = SIZE_MAX;
	size_t AOPass::ShaderIDs::projMatrix_I = SIZE_MAX;
	size_t AOPass::ShaderIDs::viewProjMatrix = SIZE_MAX;
	size_t AOPass::ShaderIDs::viewProjMatrix_I = SIZE_MAX;
	size_t AOPass::ShaderIDs::g_AOSampleKernelArray = SIZE_MAX;
	size_t AOPass::ShaderIDs::g_AOSampleCount = SIZE_MAX;
	size_t AOPass::ShaderIDs::g_AORadius = SIZE_MAX;
	size_t AOPass::ShaderIDs::g_AOIntensityMul = SIZE_MAX;
	size_t AOPass::ShaderIDs::g_AOIntensityPow = SIZE_MAX;
	size_t AOPass::ShaderIDs::g_AOIndex = SIZE_MAX;
	size_t AOPass::ShaderIDs::blitterTextureIndex = SIZE_MAX;

	AOPass::AOPass(DX12Camera* pCamera) :
		BasePass(pCamera)
	{
		ShaderIDs::g_ScreenSize = PropertyToID("g_ScreenSize");
		ShaderIDs::viewMatrix = PropertyToID("viewMatrix");
		ShaderIDs::viewMatrix_I = PropertyToID("viewMatrix_I");
		ShaderIDs::projMatrix = PropertyToID("projMatrix");
		ShaderIDs::projMatrix_I = PropertyToID("projMatrix_I");
		ShaderIDs::viewProjMatrix = PropertyToID("viewProjMatrix");
		ShaderIDs::viewProjMatrix_I = PropertyToID("viewProjMatrix_I");
		
		ShaderIDs::g_AOSampleKernelArray = PropertyToID("g_AOSampleKernelArray");
		ShaderIDs::g_AOSampleCount = PropertyToID("g_AOSampleCount");
		ShaderIDs::g_AORadius = PropertyToID("g_AORadius");
		ShaderIDs::g_AOIntensityMul = PropertyToID("g_AOIntensityMul");
		ShaderIDs::g_AOIntensityPow = PropertyToID("g_AOIntensityPow");
		ShaderIDs::g_AOIndex = PropertyToID("g_AOIndex");
		ShaderIDs::blitterTextureIndex = PropertyToID("blitterTextureIndex");
	}
	AOPass::~AOPass()
	{
		Dispose();
	}
	void AOPass::Dispose()
	{

	}

	void AOPass::Configure()
	{
		m_pAORT = CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
			static_cast<UINT64>(m_renderSize.y),
			DXGI_FORMAT_R8G8B8A8_UNORM,
			L"AO RT");

		m_shaderPasses = std::vector<ShaderPass>
		{
			ShaderPass
			{
				.Name = "AO Pass",
				.FilePath = L"Shaders\\public\\SSAO.hlsl",
				.RasterizerDesc = GetRasterizerState(RasterizerState::NoCullNoMS),
				.BlendDesc = GetBlendState(BlendState::Disabled),
				.DepthStencilDesc = GetDepthState(DepthState::Disabled)
			},
			ShaderPass
			{
				.Name = "Blit Pass",
				.FilePath = L"Shaders\\public\\Blit.hlsl",
				.VertexEntryPoint = L"BlitVS",
				.FragmentEntryPoint = L"BlitPS",
				.RasterizerDesc = GetRasterizerState(RasterizerState::NoCullNoMS),
				.BlendDesc = GetBlendState(BlendState::Disabled),
				.DepthStencilDesc = GetDepthState(DepthState::Disabled)
			}
		};

		m_pMaterial = std::make_unique<RenderMaterial>(m_shaderPasses);
		ShaderPasseIDs::AOPassID = m_pMaterial->FindPassIndex("AO Pass");
		ShaderPasseIDs::BlitPassID = m_pMaterial->FindPassIndex("Blit Pass");

		{
			RenderTargetDesc RTDesc = RenderTargetDesc
			{
				.m_renderTargetFormats = m_pAORT->GetFormat(),
				.m_numRenderTargets = 1,
				.m_depthStencilFormat = GetBufferManager()->GetCameraDepthRT()->GetFormat()
			};

			auto emplaceResult = m_PipelineStateObjects.try_emplace(ShaderPasseIDs::AOPassID);
			if (emplaceResult.second)
			{
				emplaceResult.first->second = GetPSOManager()->GetGraphicsPipelineState(m_pMaterial.get(), ShaderPasseIDs::AOPassID, RTDesc);
			}
		}
		{
			RenderTargetDesc RTDesc = RenderTargetDesc
			{
				.m_renderTargetFormats = GetBufferManager()->GetCameraColorRT()->GetFormat(),
				.m_numRenderTargets = 1,
				.m_depthStencilFormat = GetBufferManager()->GetCameraDepthRT()->GetFormat()
			};
			m_cameraColorFormat = GetBufferManager()->GetCameraColorRT()->GetFormat();

			auto emplaceResult = m_PipelineStateObjects.try_emplace(ShaderPasseIDs::BlitPassID);
			if (emplaceResult.second)
			{
				emplaceResult.first->second = GetPSOManager()->GetGraphicsPipelineState(m_pMaterial.get(), ShaderPasseIDs::BlitPassID, RTDesc);
			}
		} 

		m_pMaterial->SetConstantVariable(ShaderIDs::g_AOSampleKernelArray, GenerateSSAOSampleKernel());
		TextureManager::GetInstance().AddGlobalRT("g_AOIndex", m_pAORT->GetTexture()->GetResourceHeapIndex());

	}

	void AOPass::Execute()
	{
		UpdatePSO();
		m_pMaterial->SetConstantVariable(ShaderIDs::g_ScreenSize, GetScreenSize(Vector2(m_renderSize.x, m_renderSize.y)));
		m_pMaterial->SetConstantVariable(ShaderIDs::viewMatrix, m_pCamera->GetViewMat());
		m_pMaterial->SetConstantVariable(ShaderIDs::viewMatrix_I, m_pCamera->GetViewMat().Invert());
		m_pMaterial->SetConstantVariable(ShaderIDs::projMatrix, m_pCamera->GetProjMat());
		m_pMaterial->SetConstantVariable(ShaderIDs::projMatrix_I, m_pCamera->GetProjMat().Invert());
		m_pMaterial->SetConstantVariable(ShaderIDs::viewProjMatrix, m_pCamera->GetViewMat() * m_pCamera->GetProjMat());
		m_pMaterial->SetConstantVariable(ShaderIDs::viewProjMatrix_I, (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert());

		m_pMaterial->SetConstantVariable(ShaderIDs::g_AOSampleCount, UserData::GetInstance().aoParameter.SampleCount);
		m_pMaterial->SetConstantVariable(ShaderIDs::g_AORadius, UserData::GetInstance().aoParameter.Radius);
		m_pMaterial->SetConstantVariable(ShaderIDs::g_AOIntensityMul, UserData::GetInstance().aoParameter.IntensityMul);
		m_pMaterial->SetConstantVariable(ShaderIDs::g_AOIntensityPow, UserData::GetInstance().aoParameter.IntensityPow);

		m_pMaterial->ApplyConstantData();
	}

	void AOPass::Render()
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "AO Pass");

		Execute();

		DoCalcAO();
		//DoBlitToBackBuffer();
	}

	void AOPass::UpdatePSO()
	{
		
	}

	void AOPass::DoCalcAO()
	{
		m_pCommand->AddBarrier(m_pAORT.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_pCommand->ClearRenderTarget(m_pAORT.get(), Color::Black);

		m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize));
		m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = m_PipelineStateObjects[ShaderPasseIDs::AOPassID];
		pipelineStateData.m_renderTargets = { m_pAORT->GetTexture() };
		pipelineStateData.m_depthStencilTarget = GetBufferManager()->GetCameraDepthRT()->GetTexture();

		bool isReady = true; 
		{
			if (m_pAORT->GetTexture() == nullptr || GetBufferManager()->GetCameraDepthRT()->GetTexture() == nullptr)
			{
				ThrowRuntimeError("null texture resource");
			}
			isReady &= m_pAORT->GetTexture()->GetIsReady();
			isReady &= GetBufferManager()->GetCameraDepthRT()->GetTexture()->GetIsReady();
		}
		if (isReady)
		{
			m_pCommand->SetPipeline(pipelineStateData);
			m_pCommand->SetPipelineResource(PER_PASS_SPACE, m_pMaterial->GetPassData(ShaderPasseIDs::AOPassID).MeshResourceLayouts->m_spaces[PER_PASS_SPACE]);
			m_pCommand->SetPipelineResource(PER_FRAME_SPACE, GetRenderResource()->GetPerFrameBindResourceSpace());

			m_pCommand->DrawFullScreenTriangle();
		}

		m_pCommand->AddBarrier(m_pAORT.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	void AOPass::DoBlitToBackBuffer()
	{
		m_pCommand->AddBarrier(*GetBufferManager()->GetCameraColorRT()->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_pCommand->FlushBarrier();
		m_pCommand->ClearRenderTarget(*GetBufferManager()->GetCameraColorRT()->GetTexture(), Color(0, 0, 0, 0));

		m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize));
		m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		PipelineInfo pipelineStateData{};
		pipelineStateData.m_pipelineStateObject = m_PipelineStateObjects[ShaderPasseIDs::BlitPassID];
		pipelineStateData.m_renderTargets = { GetBufferManager()->GetCameraColorRT()->GetTexture() };
		pipelineStateData.m_depthStencilTarget = GetBufferManager()->GetCameraDepthRT()->GetTexture();

		bool isReady = true;
		{
			if (GetBufferManager()->GetCameraColorRT()->GetTexture() == nullptr || GetBufferManager()->GetCameraDepthRT()->GetTexture() == nullptr)
			{
				ThrowRuntimeError("null texture resource");
			}
			isReady &= GetBufferManager()->GetCameraColorRT()->GetTexture()->GetIsReady();
			isReady &= GetBufferManager()->GetCameraDepthRT()->GetTexture()->GetIsReady();
		}
		if (isReady)
		{
			m_pCommand->SetPipeline(pipelineStateData);
			m_pMaterial->SetConstantVariable(ShaderIDs::blitterTextureIndex, m_pAORT->GetTexture()->GetResourceHeapIndex(), ShaderPasseIDs::BlitPassID);
			m_pMaterial->ApplyConstantData();
			m_pCommand->SetPipelineResource(PER_PASS_SPACE, m_pMaterial->GetPassData(ShaderPasseIDs::BlitPassID).MeshResourceLayouts->m_spaces[PER_PASS_SPACE]);

			m_pCommand->DrawFullScreenTriangle();
		}

		m_pCommand->AddBarrier(*GetBufferManager()->GetCameraColorRT()->GetTexture(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_pCommand->FlushBarrier();
	}

	std::vector<Vector4> AOPass::GenerateSSAOSampleKernel()
	{
		std::vector<Vector4> o{};
		int maxSampleCount = 64;
		maxSampleCount = min(UserData::GetInstance().aoParameter.SampleCount, maxSampleCount);
		maxSampleCount = max(8, maxSampleCount);
		o.reserve(maxSampleCount);

		for (UINT i = 0; i < maxSampleCount; i++)
		{
			auto randomVec = Vector4(MathHelper::RandF(-1.f, 1.f), MathHelper::RandF(-1.f, 1.f), MathHelper::RandF(0.f, 1.f), 1.f);
			//randomVec.Normalize();

			auto scale = (float)i / maxSampleCount;
			scale = MathHelper::Lerp(0.01f, 1.f, scale * scale);   // 二次函数分布
			randomVec = randomVec * scale;

			o.emplace_back(std::move(randomVec));
		}

		return o;
	}
}