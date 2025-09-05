#pragma once

#include "stdafx.h"
#include "DX12Device.h"

namespace ElysiaRenderer
{
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