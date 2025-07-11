#pragma once
#include "DX12Context.h"

namespace ElysiaRenderer
{
	extern class DX12Device;

	class DX12GraphicsContext : public DX12Context
	{
		DX12GraphicsContext(DX12Device* device);
		~DX12GraphicsContext() override;
	};
}