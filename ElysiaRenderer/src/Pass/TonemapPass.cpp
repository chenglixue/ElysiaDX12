#include "stdafx.h"
#include "TonemapPass.h"

#include "lib/DX12/DX12Device.h"
#include "lib/Utility/RenderTexture.h"
#include "AMD/LPM/ColorConversion.h"

#include <stdint.h>
#define A_CPU 1
#include "RenderResource.h"
#include "AMD/LPM/ffx_a.h"
A_STATIC AF1 fs2S;
A_STATIC AF1 hdr10S;
static std::vector<UINT> ctl(24 * 4);

A_STATIC void LpmSetupOut(AU1 i, inAU4 v)
{
	for (int j = 0; j < 4; ++j) 
	{ 
		ctl[i * 4 + j] = v[j]; 
	}
}
#include "AMD/LPM/ffx_lpm.h"

namespace ElysiaRenderer
{ 
	using namespace CAULDRON_DX12;
	using namespace ElysiaHelper;

	int TonemapPass::ShaderPasseIDs::BlitPassID = -1;
	int TonemapPass::ShaderPasseIDs::TonemapPassID = -1;
	size_t TonemapPass::ShaderIDs::u_shoulder = SIZE_MAX;
	size_t TonemapPass::ShaderIDs::u_con = SIZE_MAX;
	size_t TonemapPass::ShaderIDs::u_soft = SIZE_MAX;
	size_t TonemapPass::ShaderIDs::u_con2 = SIZE_MAX;
	size_t TonemapPass::ShaderIDs::u_clip = SIZE_MAX;
	size_t TonemapPass::ShaderIDs::u_scaleOnly = SIZE_MAX;
	size_t TonemapPass::ShaderIDs::u_displayMode = SIZE_MAX;
	size_t TonemapPass::ShaderIDs::u_inputToOutputMatrix = SIZE_MAX;
	size_t TonemapPass::ShaderIDs::u_ctl = SIZE_MAX;
	size_t TonemapPass::ShaderIDs::tonemapMode = SIZE_MAX;
	size_t TonemapPass::ShaderIDs::blitterTextureIndex = SIZE_MAX;

	TonemapPass::TonemapPass(DX12Camera* pCamera) : 
		BasePass(pCamera)
	{
		ShaderIDs::u_shoulder = PropertyToID("u_shoulder");
		ShaderIDs::u_con = PropertyToID("u_con");
		ShaderIDs::u_soft = PropertyToID("u_soft");
		ShaderIDs::u_con2 = PropertyToID("u_con2");
		ShaderIDs::u_clip = PropertyToID("u_clip");
		ShaderIDs::u_scaleOnly = PropertyToID("u_scaleOnly");
		ShaderIDs::u_displayMode = PropertyToID("u_displayMode");
		ShaderIDs::u_inputToOutputMatrix = PropertyToID("u_inputToOutputMatrix");
		ShaderIDs::u_ctl = PropertyToID("u_ctl");
		ShaderIDs::tonemapMode = PropertyToID("tonemapMode");
		ShaderIDs::blitterTextureIndex = PropertyToID("blitterTextureIndex");
	}
	TonemapPass::~TonemapPass()
	{
		Dispose();
	}
	void TonemapPass::Dispose()
	{

	}
	    
	void TonemapPass::Configure()
	{
		if (!UserData::GetInstance().IsUseHDR)
		{
			m_pTempRT = CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
				static_cast<UINT64>(m_renderSize.y),
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
				L"Temp RT");
		}
		else
		{
			switch (UserData::GetInstance().HDRLevel)
			{
				case HDRQuality::Low:
				{
					m_pTempRT = CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
						static_cast<UINT64>(m_renderSize.y),
						DXGI_FORMAT_R11G11B10_FLOAT,
						L"Temp RT");
					break;
				}
				case HDRQuality::High:
				{
					m_pTempRT = CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
						static_cast<UINT64>(m_renderSize.y),
						DXGI_FORMAT_R16G16B16A16_FLOAT,
						L"Temp RT");
					break;
				}
				default:
				{
					ThrowRuntimeError("Invalid choose");
					break;
				}
			}
		}

		m_shaderPasses = std::vector<ShaderPass>
		{
			ShaderPass
			{
				.Name = "Blit Pass",
				.FilePath = L"Shaders\\public\\TonemapPass.hlsl",
				.FragmentEntryPoint = L"BlitPS",
				.RasterizerDesc = GetRasterizerState(RasterizerState::NoCullNoMS),
				.BlendDesc = GetBlendState(BlendState::Disabled),
				.DepthStencilDesc = GetDepthState(DepthState::Disabled)
			},
			ShaderPass
			{
				.Name = "Tonemap Pass",
				.FilePath = L"Shaders\\public\\TonemapPass.hlsl",
				.RasterizerDesc = GetRasterizerState(RasterizerState::NoCullNoMS),
				.BlendDesc = GetBlendState(BlendState::Disabled),
				.DepthStencilDesc = GetDepthState(DepthState::Disabled)
			}
		};

		m_pMaterial = std::make_unique<Material>(m_shaderPasses);
		ShaderPasseIDs::BlitPassID = m_pMaterial->FindPassIndex("Blit Pass");
		ShaderPasseIDs::TonemapPassID = m_pMaterial->FindPassIndex("Tonemap Pass");

		{
			RenderTargetDesc RTDesc = RenderTargetDesc
			{
				.m_renderTargetFormats = m_pTempRT->GetFormat(),
				.m_numRenderTargets = 1,
				.m_depthStencilFormat = GetBufferManager()->GetCameraDepthRT()->GetFormat()
			};

			auto emplaceResult = m_PipelineStateObjects.try_emplace(ShaderPasseIDs::BlitPassID);
			if (emplaceResult.second)
			{
				emplaceResult.first->second = GetPSOManager()->GetGraphicsPipelineState(m_pMaterial.get(), ShaderPasseIDs::BlitPassID, RTDesc);
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
			auto emplaceResult = m_PipelineStateObjects.try_emplace(ShaderPasseIDs::TonemapPassID);
			if (emplaceResult.second)
			{  
				emplaceResult.first->second = GetPSOManager()->GetGraphicsPipelineState(m_pMaterial.get(), ShaderPasseIDs::TonemapPassID, RTDesc);
			}
		}
	}
	void TonemapPass::Execute()
	{
		SetupGamutMapperMatrices( 
            ColorSpace_REC709, 
			UserData::GetInstance().colorSpace, 
            &m_inputToOutputMatrix   
        );

		varAF2(fs2R);   
		varAF2(fs2G);
		varAF2(fs2B);   
		varAF2(fs2W);
		varAF2(displayMinMaxLuminance);
		if (UserData::GetInstance().displayMode != DisplayMode::DISPLAYMODE_SDR)
		{
			const DXGI_OUTPUT_DESC1* displayInfo = CAULDRON_DX12::GetDisplayInfo();

			// Only used in fs2 modes
			fs2R[0] = displayInfo->RedPrimary[0];
			fs2R[1] = displayInfo->RedPrimary[1];
			fs2G[0] = displayInfo->GreenPrimary[0];
			fs2G[1] = displayInfo->GreenPrimary[1];
			fs2B[0] = displayInfo->BluePrimary[0];
			fs2B[1] = displayInfo->BluePrimary[1];
			fs2W[0] = displayInfo->WhitePoint[0];
			fs2W[1] = displayInfo->WhitePoint[1];
			// Only used in fs2 modes

			displayMinMaxLuminance[0] = displayInfo->MinLuminance;
			displayMinMaxLuminance[1] = displayInfo->MaxLuminance;
		}
		 
		m_shoulder = UserData::GetInstance().bShoulder;
		m_softGap = UserData::GetInstance().SoftGap;
		m_hdrMax = UserData::GetInstance().HdrMax;
		m_exposure = UserData::GetInstance().LpmExposure;
		m_contrast = UserData::GetInstance().Contrast;
		m_shoulderContrast = UserData::GetInstance().ShoulderContrast;
		m_saturation[0] = UserData::GetInstance().Saturation[0];
		m_saturation[1] = UserData::GetInstance().Saturation[1];
		m_saturation[2] = UserData::GetInstance().Saturation[2];
		m_crosstalk[0] = UserData::GetInstance().Crosstalk[0];
		m_crosstalk[1] = UserData::GetInstance().Crosstalk[1];
		m_crosstalk[2] = UserData::GetInstance().Crosstalk[2];
		
		switch (UserData::GetInstance().colorSpace)
		{
			case ColorSpace_REC709:
			{
				switch (UserData::GetInstance().displayMode)
				{
				case DisplayMode::DISPLAYMODE_SDR:
					SetLPMConfig(LPM_CONFIG_709_709);
					SetLPMColors(LPM_COLORS_709_709);
					break;

				case DisplayMode::DISPLAYMODE_FSHDR_Gamma22:
					SetLPMConfig(LPM_CONFIG_FS2RAW_709);
					SetLPMColors(LPM_COLORS_FS2RAW_709);
					break;

				case DisplayMode::DISPLAYMODE_FSHDR_SCRGB:
					fs2S = LpmFs2ScrgbScalar(displayMinMaxLuminance[0], displayMinMaxLuminance[1]);
					SetLPMConfig(LPM_CONFIG_FS2SCRGB_709);
					SetLPMColors(LPM_COLORS_FS2SCRGB_709);
					break;

				case DisplayMode::DISPLAYMODE_HDR10_2084:
					hdr10S = LpmHdr10RawScalar(displayMinMaxLuminance[1]);
					SetLPMConfig(LPM_CONFIG_HDR10RAW_709);
					SetLPMColors(LPM_COLORS_HDR10RAW_709);
					break;

				case DisplayMode::DISPLAYMODE_HDR10_SCRGB:
					hdr10S = LpmHdr10ScrgbScalar(displayMinMaxLuminance[1]);
					SetLPMConfig(LPM_CONFIG_HDR10SCRGB_709);
					SetLPMColors(LPM_COLORS_HDR10SCRGB_709);
					break;

				default:
					break;
				}
				break;
			}

			case ColorSpace_REC2020:
			{
				switch (UserData::GetInstance().displayMode)
				{
				case DisplayMode::DISPLAYMODE_SDR:
					SetLPMConfig(LPM_CONFIG_709_2020);
					SetLPMColors(LPM_COLORS_709_2020);
					break;

				case DisplayMode::DISPLAYMODE_FSHDR_Gamma22:
					SetLPMConfig(LPM_CONFIG_FS2RAW_2020);
					SetLPMColors(LPM_COLORS_FS2RAW_2020);
					break;

				case DisplayMode::DISPLAYMODE_FSHDR_SCRGB:
					fs2S = LpmFs2ScrgbScalar(displayMinMaxLuminance[0], displayMinMaxLuminance[1]);
					SetLPMConfig(LPM_CONFIG_FS2SCRGB_2020);
					SetLPMColors(LPM_COLORS_FS2SCRGB_2020);
					break;

				case DisplayMode::DISPLAYMODE_HDR10_2084:
					hdr10S = LpmHdr10RawScalar(displayMinMaxLuminance[1]);
					SetLPMConfig(LPM_CONFIG_HDR10RAW_2020);
					SetLPMColors(LPM_COLORS_HDR10RAW_2020);
					break;

				case DisplayMode::DISPLAYMODE_HDR10_SCRGB:
					hdr10S = LpmHdr10ScrgbScalar(displayMinMaxLuminance[1]);
					SetLPMConfig(LPM_CONFIG_HDR10SCRGB_2020);
					SetLPMColors(LPM_COLORS_HDR10SCRGB_2020);
					break;

				default:
					break;
				}
				break;
			}

			case ColorSpace_P3:
			{
				switch (UserData::GetInstance().displayMode)
				{
				case DisplayMode::DISPLAYMODE_SDR:
					SetLPMConfig(LPM_CONFIG_709_P3);
					SetLPMColors(LPM_COLORS_709_P3);
					break;

				case DisplayMode::DISPLAYMODE_FSHDR_Gamma22:
					SetLPMConfig(LPM_CONFIG_FS2RAW_P3);
					SetLPMColors(LPM_COLORS_FS2RAW_P3);
					break;

				case DisplayMode::DISPLAYMODE_FSHDR_SCRGB:
					fs2S = LpmFs2ScrgbScalar(displayMinMaxLuminance[0], displayMinMaxLuminance[1]);
					SetLPMConfig(LPM_CONFIG_FS2SCRGB_P3);
					SetLPMColors(LPM_COLORS_FS2SCRGB_P3);
					break;

				case DisplayMode::DISPLAYMODE_HDR10_2084:
					hdr10S = LpmHdr10RawScalar(displayMinMaxLuminance[1]);
					SetLPMConfig(LPM_CONFIG_HDR10RAW_P3);
					SetLPMColors(LPM_COLORS_HDR10RAW_P3);
					break;

				case DisplayMode::DISPLAYMODE_HDR10_SCRGB:
					hdr10S = LpmHdr10ScrgbScalar(displayMinMaxLuminance[1]);
					SetLPMConfig(LPM_CONFIG_HDR10SCRGB_P3);
					SetLPMColors(LPM_COLORS_HDR10SCRGB_P3);
					break;

				default:
					break;
				}
				break;
			}
		}
		
		LpmSetup(m_shoulder, m_con, m_soft, m_con2, m_clip, m_scaleOnly,
			m_xyRedW, m_xyGreenW, m_xyBlueW, m_xyWhiteW,
			m_xyRedO, m_xyGreenO, m_xyBlueO, m_xyWhiteO,
			m_xyRedC, m_xyGreenC, m_xyBlueC, m_xyWhiteC,
			m_scaleC,
			m_softGap, m_hdrMax, m_exposure, m_contrast, m_shoulderContrast,
			m_saturation, m_crosstalk);

		
		m_pMaterial->SetConstantVariable(ShaderIDs::u_shoulder, m_shoulder, ShaderPasseIDs::TonemapPassID);
		m_pMaterial->SetConstantVariable(ShaderIDs::u_con2, m_con, ShaderPasseIDs::TonemapPassID);
		m_pMaterial->SetConstantVariable(ShaderIDs::u_soft, m_soft, ShaderPasseIDs::TonemapPassID);
		m_pMaterial->SetConstantVariable(ShaderIDs::u_con2, m_con2, ShaderPasseIDs::TonemapPassID);
		m_pMaterial->SetConstantVariable(ShaderIDs::u_clip, m_clip, ShaderPasseIDs::TonemapPassID);
		m_pMaterial->SetConstantVariable(ShaderIDs::u_scaleOnly, m_scaleOnly, ShaderPasseIDs::TonemapPassID);
		m_pMaterial->SetConstantVariable(ShaderIDs::u_displayMode, (UINT)UserData::GetInstance().displayMode, ShaderPasseIDs::TonemapPassID);
		m_pMaterial->SetConstantVariable(ShaderIDs::u_inputToOutputMatrix, m_inputToOutputMatrix, ShaderPasseIDs::TonemapPassID);
		m_pMaterial->SetConstantVariable(ShaderIDs::u_ctl, ctl, ShaderPasseIDs::TonemapPassID);
		m_pMaterial->SetConstantVariable(ShaderIDs::tonemapMode, (UINT)UserData::GetInstance().tonemapMode, ShaderPasseIDs::TonemapPassID);
		m_pMaterial->ApplyConstantData();
	}

	void TonemapPass::Render()
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "Tonemap Pass");

		Execute();
		{
			m_pCommand->AddBarrier(m_pTempRT.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
			m_pCommand->ClearRenderTarget(m_pTempRT.get(), Color::Black);

			m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize));
			m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			PipelineInfo pipelineStateData{};
			pipelineStateData.m_pipelineStateObject = m_PipelineStateObjects[ShaderPasseIDs::BlitPassID];
			pipelineStateData.m_renderTargets = { m_pTempRT->GetTexture() };
			pipelineStateData.m_depthStencilTarget = GetBufferManager()->GetCameraDepthRT()->GetTexture();

			bool isReady = true;
			{
				if (m_pTempRT->GetTexture() == nullptr || GetBufferManager()->GetCameraDepthRT()->GetTexture() == nullptr)
				{
					ThrowRuntimeError("null texture resource");
				}
				isReady &= m_pTempRT->GetTexture()->GetIsReady();
				isReady &= GetBufferManager()->GetCameraDepthRT()->GetTexture()->GetIsReady();
			}
			if (isReady)
			{
				m_pCommand->SetPipeline(pipelineStateData);
				m_pMaterial->SetConstantVariable("blitterTextureIndex", GetBufferManager()->GetCameraColorRT()->GetTexture()->GetResourceHeapIndex(), ShaderPasseIDs::BlitPassID);
				m_pMaterial->ApplyConstantData();
				m_pCommand->SetPipelineResource(PER_PASS_SPACE, m_pMaterial->GetPassData(ShaderPasseIDs::BlitPassID).MeshResourceLayouts->m_spaces[PER_PASS_SPACE]);

				m_pCommand->DrawFullScreenTriangle();
			}

			m_pCommand->AddBarrier(m_pTempRT.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}

		{
			auto cameraColorRT = GetBufferManager()->GetCameraColorRT();

			m_pCommand->AddBarrier(cameraColorRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
			m_pCommand->ClearRenderTarget(cameraColorRT, Color::Black);

			m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize));
			m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			PipelineInfo pipelineStateData{};
			pipelineStateData.m_pipelineStateObject = m_PipelineStateObjects[ShaderPasseIDs::TonemapPassID];
			pipelineStateData.m_renderTargets = { cameraColorRT->GetTexture() };
			pipelineStateData.m_depthStencilTarget = GetBufferManager()->GetCameraDepthRT()->GetTexture();

			bool isReady = true;
			{
				if (cameraColorRT->GetTexture() == nullptr || GetBufferManager()->GetCameraDepthRT()->GetTexture() == nullptr)
				{
					ThrowRuntimeError("null texture resource");
				}
				isReady &= cameraColorRT->GetTexture()->GetIsReady();
				isReady &= GetBufferManager()->GetCameraDepthRT()->GetTexture()->GetIsReady();
			}
			if (isReady)
			{
				m_pCommand->SetPipeline(pipelineStateData);
				m_pMaterial->SetConstantVariable(ShaderIDs::blitterTextureIndex, m_pTempRT->GetTexture()->GetResourceHeapIndex(), ShaderPasseIDs::TonemapPassID);
				m_pMaterial->ApplyConstantData();
				m_pCommand->SetPipelineResource(PER_PASS_SPACE, m_pMaterial->GetPassData(ShaderPasseIDs::TonemapPassID).MeshResourceLayouts->m_spaces[PER_PASS_SPACE]);

				m_pCommand->DrawFullScreenTriangle();
			}

			m_pCommand->AddBarrier(cameraColorRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
	}

	void TonemapPass::SetLPMConfig(bool con, bool soft, bool con2, bool clip, bool scaleOnly)
	{
		m_con = con;
		m_soft = soft;
		m_con2 = con2;
		m_clip = clip;
		m_scaleOnly = scaleOnly;
	}
	void TonemapPass::SetLPMColors(
		float xyRedW[2], float xyGreenW[2], float xyBlueW[2], float xyWhiteW[2],
		float xyRedO[2], float xyGreenO[2], float xyBlueO[2], float xyWhiteO[2],
		float xyRedC[2], float xyGreenC[2], float xyBlueC[2], float xyWhiteC[2],
		float scaleC
	)
	{
		m_xyRedW[0] = xyRedW[0]; m_xyRedW[1] = xyRedW[1];
		m_xyGreenW[0] = xyGreenW[0]; m_xyGreenW[1] = xyGreenW[1];
		m_xyBlueW[0] = xyBlueW[0]; m_xyBlueW[1] = xyBlueW[1];
		m_xyWhiteW[0] = xyWhiteW[0]; m_xyWhiteW[1] = xyWhiteW[1];

		m_xyRedO[0] = xyRedO[0]; m_xyRedO[1] = xyRedO[1];
		m_xyGreenO[0] = xyGreenO[0]; m_xyGreenO[1] = xyGreenO[1];
		m_xyBlueO[0] = xyBlueO[0]; m_xyBlueO[1] = xyBlueO[1];
		m_xyWhiteO[0] = xyWhiteO[0]; m_xyWhiteO[1] = xyWhiteO[1];

		m_xyRedC[0] = xyRedC[0]; m_xyRedC[1] = xyRedC[1];
		m_xyGreenC[0] = xyGreenC[0]; m_xyGreenC[1] = xyGreenC[1];
		m_xyBlueC[0] = xyBlueC[0]; m_xyBlueC[1] = xyBlueC[1];
		m_xyWhiteC[0] = xyWhiteC[0]; m_xyWhiteC[1] = xyWhiteC[1];

		m_scaleC = scaleC;
	}

	void TonemapPass::UpdatePSO()
	{
		// if(m_cameraColorFormat != GetBufferManager()->GetCameraColorRT()->GetFormat())
		// {
		// 	if(m_pTempRT)
		// 	{
		// 		m_pTempRT.reset();
		// 		
		// 		m_pTempRT = CreateRenderTexture(static_cast<UINT64>(m_renderSize.x),
		// 		static_cast<UINT64>(m_renderSize.y),
		// 		GetBufferManager()->GetCameraColorRT()->GetFormat(),
		// 		L"Temp RT");
		// 	}
		// 	
		// 	{
		// 		RenderTargetDesc RTDesc = RenderTargetDesc
		// 		{
		// 			.m_renderTargetFormats = GetBufferManager()->GetCameraColorRT()->GetFormat(),
		// 			.m_numRenderTargets = 1,
		// 			.m_depthStencilFormat = GetBufferManager()->GetCameraDepthRT()->GetFormat()
		// 		};
		// 		m_cameraColorFormat = GetBufferManager()->GetCameraColorRT()->GetFormat();
		//
		// 		m_PipelineStateObjects[ShaderPasseIDs::TonemapPassID] = GetPSOManager()->GetGraphicsPipelineState(m_pMaterial.get(), ShaderPasseIDs::TonemapPassID, RTDesc);
		//
		// 	}
		// 	{
		// 		RenderTargetDesc RTDesc = RenderTargetDesc
		// 		{
		// 			.m_renderTargetFormats = m_pTempRT->GetFormat(),
		// 			.m_numRenderTargets = 1,
		// 			.m_depthStencilFormat = GetBufferManager()->GetCameraDepthRT()->GetFormat()
		// 		};
		//
		// 		m_PipelineStateObjects[ShaderPasseIDs::BlitPassID] = GetPSOManager()->GetGraphicsPipelineState(m_pMaterial.get(), ShaderPasseIDs::BlitPassID, RTDesc);
		// 	}
		// }
	}

}