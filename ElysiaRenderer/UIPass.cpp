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

		auto cameraColorRT = GetBufferManager()->GetCameraColorRT();

		m_pCommand->AddBarrier(*cameraColorRT->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_pCommand->FlushBarrier();

		m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize.x, m_renderSize.y));

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_pCommand->GetCommandList());

		m_pCommand->AddBarrier(*cameraColorRT->GetTexture(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_pCommand->FlushBarrier();
	}

	void UIPass::Dispose()
	{

	}

	void UIPass::AddUIItems()
	{
		auto& pUserData = UserData::GetInstance();

		if (ImGui::CollapsingHeader("Light"))
		{
			ImGui::ColorEdit3("Color", (float*)&pUserData.lightColor);
			ImGui::DragFloat3("Direction", (float*)&pUserData.lightDir, 1, -1, 1);
			ImGui::SliderFloat("Intensity", &pUserData.lightIntensity, 0, 100);

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
	}
}