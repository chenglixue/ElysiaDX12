#include "stdafx.h"
#include "UIPass.h"

#include "lib/DX12/DX12Device.h"
#include "lib/Utility/RenderTexture.h"
#include "RenderResource.h"
#include "lib/Event/Messager.h"

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
		UpdatePSO();
		
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

	void UIPass::UpdatePSO()
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
			debugModeIndex = std::clamp(debugModeIndex, 0, static_cast<int>(magic_enum::enum_count<DebugMode>()));
			pUserData.debugMode = (DebugMode)debugModeIndex;
		}

		if (ImGui::CollapsingHeader("Light"))
		{
			ImGui::ColorEdit3("Color", (float*)&pUserData.lightColor);
			ImGui::DragFloat3("Direction", (float*)&pUserData.lightDir, 1, -1, 1, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderFloat("Intensity", &pUserData.lightIntensity, 0, 20, "%.3f", ImGuiSliderFlags_AlwaysClamp);

			int shadowTypeIndex = (int)pUserData.shadowType;
			ImGui::Combo("Shadow Type", &shadowTypeIndex,
				StringViewToChar(magic_enum::enum_names<ShadowType>().data(), magic_enum::enum_count<ShadowType>()).data(),
				(int)magic_enum::enum_count<ShadowType>());
			shadowTypeIndex = std::clamp(shadowTypeIndex, 0, static_cast<int>(magic_enum::enum_count<ShadowType>()));
			if (ImGui::IsItemEdited())
			{
				Messager::GetInstance().Broadcast(MessageID::ShadowType, shadowTypeIndex);
			}
			pUserData.shadowType = (ShadowType)shadowTypeIndex;

			int shadowQualityIndex = (int)pUserData.shadowQuality;
			ImGui::Combo("Shadow Quality", &shadowQualityIndex,
				StringViewToChar(magic_enum::enum_names<ShadowQuality>().data(), magic_enum::enum_count<ShadowQuality>()).data(),
				(int)magic_enum::enum_count<ShadowQuality>());
			shadowQualityIndex = std::clamp(shadowQualityIndex, 0, static_cast<int>(magic_enum::enum_count<ShadowQuality>()));
			pUserData.shadowQuality = (ShadowQuality)shadowQualityIndex;

			ImGui::SliderFloat("Shadow Depth Bias", &pUserData.shadowDepthBias, 0, 10, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderFloat("Shadow Slope Depth Bias", &pUserData.shadowSlopeDepthBias, 0, 10, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderFloat("Shadow Max Slope Depth Bias", &pUserData.shadowMaxSlopeDepthBias, 0, 10, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		} 
		     
		if (ImGui::CollapsingHeader("PBR Data"))
		{
			ImGui::ColorEdit3("Base Color Tint", (float*)&pUserData.BaseColorTint, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_HDR);
			ImGui::SliderFloat("Opacity", &pUserData.Opacity, 0.f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderFloat("Cutoff", &pUserData.Cutoff, 0.f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderFloat("Normal Intensity", &pUserData.NormalIntensity, 0.f, 5.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderFloat("Metallic Intensity", &pUserData.MetallicIntensity, 0.f, 5.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderFloat("Roughness Intensity", &pUserData.RoughnessIntensity, 0.f, 5.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderFloat("Ambient Cubemap Intensity", &pUserData.AmbientCubemapIntensity, 0.f, 2.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::ColorEdit3("Ambient Cubemap Tint", (float*)&pUserData.AmbientCubemapTint);
		}

		if (ImGui::CollapsingHeader("HDR"))
		{ 
			ImGui::Checkbox("Is Enable HDR", &pUserData.IsUseHDR);

			int HDRQualityIndex = (int)pUserData.HDRLevel;
			ImGui::Combo("HDR Quality", &HDRQualityIndex,
				StringViewToChar(magic_enum::enum_names<HDRQuality>().data(), magic_enum::enum_count<HDRQuality>()).data(), (int)magic_enum::enum_count<HDRQuality>());
			HDRQualityIndex = std::clamp(HDRQualityIndex, 0, static_cast<int>(magic_enum::enum_count<HDRQuality>()));
			pUserData.HDRLevel = (HDRQuality)HDRQualityIndex;

			int tonemapModeIndex = (int)pUserData.tonemapMode;
			ImGui::Combo("Tonemap Mode", &tonemapModeIndex, 
				StringViewToChar(magic_enum::enum_names<TonemapMode>().data(), magic_enum::enum_count<TonemapMode>()).data(), (int)magic_enum::enum_count<TonemapMode>());
			tonemapModeIndex = std::clamp(tonemapModeIndex, 0, static_cast<int>(magic_enum::enum_count<TonemapMode>()));
			pUserData.tonemapMode = (TonemapMode)tonemapModeIndex;

			int displayModeIndex = (int)pUserData.displayMode;
			ImGui::Combo("Display Mode", &displayModeIndex,
				StringViewToChar(magic_enum::enum_names<CAULDRON_DX12::DisplayMode>().data(), magic_enum::enum_count<CAULDRON_DX12::DisplayMode>()).data(), (int)magic_enum::enum_count<CAULDRON_DX12::DisplayMode>());
			displayModeIndex = std::clamp(displayModeIndex, 0, static_cast<int>(magic_enum::enum_count<CAULDRON_DX12::DisplayMode>()));
			pUserData.displayMode = (CAULDRON_DX12::DisplayMode)displayModeIndex;

			int colorSpaceIndex = (int)pUserData.colorSpace;
			ImGui::Combo("Color space", &colorSpaceIndex,
				StringViewToChar(magic_enum::enum_names<ColorSpace>().data(), magic_enum::enum_count<ColorSpace>()).data(), (int)magic_enum::enum_count<ColorSpace>());
			colorSpaceIndex = std::clamp(colorSpaceIndex, 0, static_cast<int>(magic_enum::enum_count<ColorSpace>()));
			pUserData.colorSpace = (ColorSpace)colorSpaceIndex;

			ImGui::Checkbox("Shoulder", &pUserData.bShoulder);
			ImGui::SliderFloat("Soft Gap", &pUserData.SoftGap, 0.0f, 0.5f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderFloat("HDR Max", &pUserData.HdrMax, 8.0f, 2048.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderFloat("LPM Exposure", &pUserData.LpmExposure, 3.0f, 11.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderFloat("Contrast", &pUserData.Contrast, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderFloat("Shoulder Contrast", &pUserData.ShoulderContrast, 1.0f, 1.2f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderFloat3("Saturation", &pUserData.Saturation[0], 0.0f, 2.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderFloat3("Crosstalk", &pUserData.Crosstalk[0], 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		}

		if (ImGui::CollapsingHeader("AO"))
		{

			ImGui::Checkbox("Is Enable AO", &pUserData.aoParameter.IsEnableAO);

			auto t1 = static_cast<int>(pUserData.aoParameter.SampleCount);
			ImGui::SliderInt("AO Sample Count", &t1, 0, 256, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			pUserData.aoParameter.SampleCount = static_cast<UINT>(t1);

			ImGui::SliderFloat("AO Radius", &pUserData.aoParameter.Radius, 0, 10, "%.3f", ImGuiSliderFlags_AlwaysClamp);

			ImGui::SliderFloat("AO Intensity", &pUserData.aoParameter.IntensityMul, 0, 2, "%.3f", ImGuiSliderFlags_AlwaysClamp);

			ImGui::SliderFloat("AO Pow", &pUserData.aoParameter.IntensityPow, 0, 2, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		}
	}
}   