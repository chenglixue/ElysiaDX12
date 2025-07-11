#include "DX12GraphicsContext.h"

namespace ElysiaRenderer
{
	extern class DX12Device;

	DX12GraphicsContext::DX12GraphicsContext(DX12Device* device) : 
		DX12Context(device, D3D12_COMMAND_LIST_TYPE_DIRECT)
	{

	}

	DX12GraphicsContext::~DX12GraphicsContext()
	{

	}


}