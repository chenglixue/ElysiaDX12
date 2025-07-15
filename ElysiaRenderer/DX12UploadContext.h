#pragma once
#include "stdafx.h"
#include "DX12Context.h"
#include "DX12BufferResource.h"

namespace ElysiaRenderer
{
	extern class DX12Device;
	class DX12UploadContext : public DX12Context
	{
	public:
		DX12UploadContext(DX12Device* device, std::unique_ptr<DX12BufferResource> textureUpload);
	private:
	};
}