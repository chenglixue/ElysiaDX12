#include "stdafx.h"
#include "TonemapPass.h"

#include "Runtime/Core/DX12GraphicsContext.h"
#include "Runtime/Core/DX12Shader.h"
#include "Runtime/Core/DX12Device.h"
#include "Runtime/Core/DX12TextureBuffer.h"
#include "Runtime/Core/SwapChain.h"

#include "Runtime/RenderCore/RenderResource.h"
#include "Runtime/RenderCore/RenderTexture.h"
#include "Runtime/RenderCore/Material.h"
#include "Runtime/RenderCore/PSOManager.h"
#include "Runtime/RenderCore/RenderTargetManager.h"
#include "Runtime/RenderCore/ShaderVariantManager.h"

#include "AMD/libs/vectormath/vectormath.hpp"
#include "src/ThirdParty/FreesyncHDR.h"
#include "src/ThirdParty/ColorConversion.h"

#include <stdint.h>

#include "Editor/UserData.h"
#include "Programs/PIXHelper.h"
#define A_CPU 1
#include "BloomPass.h"
#include "src/ThirdParty/ffx_a.h"
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

#include "src/ThirdParty/ffx_lpm.h"

namespace ElysiaRenderer
{
    int TonemapPass::ShaderPasseIDs::TonemapPassID = -1;

    TonemapPass::TonemapPass()
        : BasePass()
    {
    }

    TonemapPass::~TonemapPass()
    {
    }

    void TonemapPass::Dispose()
    {

    }

    void TonemapPass::Configure()
    {
        m_shaderPasses =
        {
            ShaderPass
            {
                .Name = "Tonemap Pass",
                .FilePath = L"Shaders\\public\\CS_Tonemap.hlsl",
                .IsComputeShader = true,
                .ComputeEntryPoint = L"Tonemap",
            },

        };

        if (!m_pMaterial)
        {
            m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        }
        ShaderPasseIDs::TonemapPassID = m_pMaterial->FindPassIndex("Tonemap Pass");
        UpdatePipeline();
    }

    void TonemapPass::Render(FrameContext& context)
    {
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;

        auto passID = ShaderPasseIDs::TonemapPassID;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
                                                                 passID)
                                                             .pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_PASS_SPACE);

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

        LpmSetup(m_shoulder,
                 m_con,
                 m_soft,
                 m_con2,
                 m_clip,
                 m_scaleOnly,
                 m_xyRedW,
                 m_xyGreenW,
                 m_xyBlueW,
                 m_xyWhiteW,
                 m_xyRedO,
                 m_xyGreenO,
                 m_xyBlueO,
                 m_xyWhiteO,
                 m_xyRedC,
                 m_xyGreenC,
                 m_xyBlueC,
                 m_xyWhiteC,
                 m_scaleC,
                 m_softGap,
                 m_hdrMax,
                 m_exposure,
                 m_contrast,
                 m_shoulderContrast,
                 m_saturation,
                 m_crosstalk);

        m_pCommand->AddBarrier(m_pDisplayRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        {
            m_pMaterial->SetBool(ShaderIDs::u_shoulder, m_shoulder);
            m_pMaterial->SetBool(ShaderIDs::u_con2, m_con);
            m_pMaterial->SetBool(ShaderIDs::u_soft, m_soft);
            m_pMaterial->SetBool(ShaderIDs::u_con2, m_con2);
            m_pMaterial->SetBool(ShaderIDs::u_clip, m_clip);
            m_pMaterial->SetBool(ShaderIDs::u_scaleOnly, m_scaleOnly);
            m_pMaterial->SetUInt(ShaderIDs::u_displayMode,
                                 (UINT)m_pSwaiChain->GetDisplayMode());
            m_pMaterial->SetMatrix(ShaderIDs::u_inputToOutputMatrix, m_inputToOutputMatrix);
            m_pMaterial->SetUINTArray(ShaderIDs::u_ctl, ctl);
            m_pMaterial->SetUInt(ShaderIDs::tonemapMode,
                                 (UINT)UserData::GetInstance().tonemapMode);
            m_pMaterial->SetFloat4(ShaderIDs::g_DestSize,
                                   GetScreenSize(Vector2(m_displaySize.x, m_displaySize.y)));
            m_pMaterial->SetUInt(ShaderIDs::g_DestTextureIndex,
                                 m_pDisplayRT->GetResourceHeapIndex());
            m_pMaterial->SetFloat(ShaderIDs::g_LocalExposure,
                                  UserData::GetInstance().localExposure);
            m_pMaterial->SetUInt(ShaderIDs::g_BloomTexIndex,
                                 RenderTargetManager::GetInstance().GetRenderTexture(RenderResource::GetInstance().
                                                                                     GetPropertyName(
                                                                                         BloomPass::RenderTextureIDs::BloomUpSampleRTID)
                                                                                     + std::to_wstring(0))->
                                                                    GetUAVResourceHeapIndex());
            m_pMaterial->SetFloat(ShaderIDs::g_BloomIntensity,
                                  UserData::GetInstance().bloomParameter.intensity,
                                  passID);

            auto& passData = m_pMaterial->GetPassData(ShaderPasseIDs::TonemapPassID);
            SetSpaceResource(passData, PER_FRAME_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_pDisplayRT->GetWidth(), threadGroupSize.x),
                                 CeilDivide(m_pDisplayRT->GetHeight(), threadGroupSize.y),
                                 threadGroupSize.z);
        }

        m_pCommand->AddBarrier(m_pDisplayRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }

    void TonemapPass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        {
            std::vector<std::wstring> enableKeywords{};

            auto& passData = m_pMaterial->GetPassData(ShaderPasseIDs::TonemapPassID);
            auto VariantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

            passData.pPipelineStateObject = PSOManager::GetInstance().GetComputePipelineState(
                m_pDevice,
                m_pMaterial.get(),
                ShaderPasseIDs::TonemapPassID);
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
        float xyRedW[2],
        float xyGreenW[2],
        float xyBlueW[2],
        float xyWhiteW[2],
        float xyRedO[2],
        float xyGreenO[2],
        float xyBlueO[2],
        float xyWhiteO[2],
        float xyRedC[2],
        float xyGreenC[2],
        float xyBlueC[2],
        float xyWhiteC[2],
        float scaleC
        )
    {
        m_xyRedW[0] = xyRedW[0];
        m_xyRedW[1] = xyRedW[1];
        m_xyGreenW[0] = xyGreenW[0];
        m_xyGreenW[1] = xyGreenW[1];
        m_xyBlueW[0] = xyBlueW[0];
        m_xyBlueW[1] = xyBlueW[1];
        m_xyWhiteW[0] = xyWhiteW[0];
        m_xyWhiteW[1] = xyWhiteW[1];

        m_xyRedO[0] = xyRedO[0];
        m_xyRedO[1] = xyRedO[1];
        m_xyGreenO[0] = xyGreenO[0];
        m_xyGreenO[1] = xyGreenO[1];
        m_xyBlueO[0] = xyBlueO[0];
        m_xyBlueO[1] = xyBlueO[1];
        m_xyWhiteO[0] = xyWhiteO[0];
        m_xyWhiteO[1] = xyWhiteO[1];

        m_xyRedC[0] = xyRedC[0];
        m_xyRedC[1] = xyRedC[1];
        m_xyGreenC[0] = xyGreenC[0];
        m_xyGreenC[1] = xyGreenC[1];
        m_xyBlueC[0] = xyBlueC[0];
        m_xyBlueC[1] = xyBlueC[1];
        m_xyWhiteC[0] = xyWhiteC[0];
        m_xyWhiteC[1] = xyWhiteC[1];

        m_scaleC = scaleC;
    }


}