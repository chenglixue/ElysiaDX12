#include "stdafx.h"
#include "DX12UI.h"


namespace ElysiaEditor
{
	
	DX12UI::DX12UI()
	{
		InitContext();
	}
	DX12UI::~DX12UI()
	{
		// ImGui_ImplDX12_Shutdown();
		// ImGui_ImplWin32_Shutdown();
		// ImGui::DestroyContext();
	}

	void DX12UI::InitContext()
	{
		// IMGUI_CHECKVERSION();
		// ImGui::CreateContext();
		// ImGuiIO& io = ImGui::GetIO();
		// ImGui::StyleColorsDark();

	}

	void DX12UI::InitDescriptor(HWND windowHandle, ElysiaCore::DX12Device* device)
	{
		auto UIDescriptor0 = device->GetImguiDescriptor(0);
		auto UIDescriptor1 = device->GetImguiDescriptor(1);

		// ImGui_ImplWin32_Init(windowHandle);
		// ImGui_ImplDX12_Init(device->GetDevice(), NUM_FRAMES_IN_FLIGHT,
		// 	DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, nullptr,
		// 	UIDescriptor0.GetCPUHandle(), UIDescriptor0.GetGPUHandle(), UIDescriptor1.GetCPUHandle(), UIDescriptor1.GetGPUHandle());

	}
}