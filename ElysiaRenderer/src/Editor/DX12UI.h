#pragma once
#include "Runtime/Core/DX12Device.h"

namespace ElysiaRenderer
{
	using namespace ElysiaCore;
	
	class DX12UI
	{
	public:
		DX12UI();
		DX12UI(DX12UI&& rhs) = default;
		~DX12UI();

		void InitContext();
		void InitDescriptor(HWND windowHandle, DX12Device* device);
	};
}