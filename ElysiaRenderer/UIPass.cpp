#include "stdafx.h"
#include "UIPass.h"

#include "DX12Device.h"
#include "RenderTexture.h"
#include "RenderResource.h"

namespace ElysiaRenderer
{
	UIPass::~UIPass()
	{
		Dispose();
	}

	void UIPass::Configure()
	{

	}
	void UIPass::Execute()
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		AddUIItems();
		
	}
	void UIPass::Render()
	{
		PIXHelper pix(m_pCommand->GetCommandList(), "UI Pass");

		Execute();

		ImGui::Render();
	}

	void UIPass::Dispose()
	{

	}

	void UIPass::AddUIItems()
	{
		auto& pUserData = UserData::GetInstance();

		if (ImGui::CollapsingHeader("Debug"))
		{
			int debugModeIndex = (int)pUserData.debugMode;
			ImGui::Combo("Debug Mode", &debugModeIndex,
				StringViewToChar(magic_enum::enum_names<DebugMode>().data(), magic_enum::enum_count<DebugMode>()).data(),
				(int)magic_enum::enum_count<DebugMode>());
			pUserData.debugMode = (DebugMode)debugModeIndex;
		}

		if (ImGui::CollapsingHeader("Light"))
		{
			ImGui::ColorEdit3("Color", (float*)&pUserData.lightColor);
			ImGui::DragFloat3("Direction", (float*)&pUserData.lightDir, 1, -1, 1);
			ImGui::SliderFloat("Intensity", &pUserData.lightIntensity, 0, 10);

			int shadowTypeIndex = (int)pUserData.shadowType;
			ImGui::Combo("Shadow Type", &shadowTypeIndex,
				StringViewToChar(magic_enum::enum_names<ShadowType>().data(), magic_enum::enum_count<ShadowType>()).data(),
				(int)magic_enum::enum_count<ShadowType>());
			pUserData.shadowType = (ShadowType)shadowTypeIndex;

			int shadowQualityIndex = (int)pUserData.shadowQuality;
			ImGui::Combo("Shadow Quality", &shadowQualityIndex,
				StringViewToChar(magic_enum::enum_names<ShadowQuality>().data(), magic_enum::enum_count<ShadowQuality>()).data(),
				(int)magic_enum::enum_count<ShadowQuality>());
			pUserData.shadowQuality = (ShadowQuality)shadowQualityIndex;

			ImGui::SliderFloat("Shadow Depth Bias", &pUserData.shadowDepthBias, 0, 10);
			ImGui::SliderFloat("Shadow Slope Depth Bias", &pUserData.shadowSlopeDepthBias, 0, 10);
			ImGui::SliderFloat("Shadow Max Slope Depth Bias", &pUserData.shadowMaxSlopeDepthBias, 0, 10);
		} 
		     
		if (ImGui::CollapsingHeader("PBR Data"))
		{
			ImGui::ColorEdit3("Base Color Tint", (float*)&pUserData.BaseColorTint);
			ImGui::SliderFloat("Opacity", &pUserData.Opacity, 0.f, 1.f);
			ImGui::SliderFloat("Cutoff", &pUserData.Cutoff, 0.f, 1.f);
			ImGui::SliderFloat("Normal Intensity", &pUserData.NormalIntensity, 0.f, 5.f);
			ImGui::SliderFloat("Metallic Intensity", &pUserData.MetallicIntensity, 0.f, 5.f);
			ImGui::SliderFloat("Roughness Intensity", &pUserData.RoughnessIntensity, 0.f, 5.f);
			ImGui::SliderFloat("Ambient Cubemap Intensity", &pUserData.AmbientCubemapIntensity, 0.f, 2.f);
			ImGui::ColorEdit3("Ambient Cubemap Tint", (float*)&pUserData.AmbientCubemapTint);
		}

		if (ImGui::CollapsingHeader("HDR"))
		{ 
			ImGui::Checkbox("Is Enable HDR", &pUserData.IsUseHDR);

			int HDRQualityIndex = (int)pUserData.HDRLevel;
			ImGui::Combo("HDR Quality", &HDRQualityIndex,
				StringViewToChar(magic_enum::enum_names<HDRQuality>().data(), magic_enum::enum_count<HDRQuality>()).data(), (int)magic_enum::enum_count<HDRQuality>());
			pUserData.HDRLevel = (HDRQuality)HDRQualityIndex;

			int tonemapModeIndex = (int)pUserData.tonemapMode;
			ImGui::Combo("Tonemap Mode", &tonemapModeIndex,
				StringViewToChar(magic_enum::enum_names<TonemapMode>().data(), magic_enum::enum_count<TonemapMode>()).data(), (int)magic_enum::enum_count<TonemapMode>());
			pUserData.tonemapMode = (TonemapMode)tonemapModeIndex;

			int colorSpaceIndex = (int)pUserData.colorSpace;
			ImGui::Combo("Color space", &colorSpaceIndex,
				StringViewToChar(magic_enum::enum_names<ColorSpace>().data(), magic_enum::enum_count<ColorSpace>()).data(), (int)magic_enum::enum_count<ColorSpace>());
			pUserData.colorSpace = (ColorSpace)colorSpaceIndex;

			ImGui::Checkbox("Shoulder", &pUserData.bShoulder);
			ImGui::SliderFloat("Soft Gap", &pUserData.SoftGap, 0.0f, 0.5f);
			ImGui::SliderFloat("HDR Max", &pUserData.HdrMax, 8.0f, 2048.0f);
			ImGui::SliderFloat("LPM Exposure", &pUserData.LpmExposure, 3.0f, 11.0f);
			ImGui::SliderFloat("Contrast", &pUserData.Contrast, 0.0f, 1.0f);
			ImGui::SliderFloat("Shoulder Contrast", &pUserData.ShoulderContrast, 1.0f, 1.2f);
			ImGui::SliderFloat3("Saturation", &pUserData.Saturation[0], 0.0f, 2.0f);
			ImGui::SliderFloat3("Crosstalk", &pUserData.Crosstalk[0], 0.0f, 1.0f);
		}

		if (ImGui::CollapsingHeader("AO"))
		{

			ImGui::Checkbox("Is Enable AO", &pUserData.aoParameter.IsEnableAO);

			auto t1 = (int)pUserData.aoParameter.SampleCount;
			ImGui::SliderInt("AO Sample Count", &t1, 0, 256);
			pUserData.aoParameter.SampleCount = (UINT)t1;

			ImGui::SliderFloat("AO Radius", &pUserData.aoParameter.Radius, 0, 10);

			ImGui::SliderFloat("AO Intensity", &pUserData.aoParameter.IntensityMul, 0, 2);

			ImGui::SliderFloat("AO Pow", &pUserData.aoParameter.IntensityPow, 0, 2);
		}
	}
}   