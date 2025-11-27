#pragma once
#include "Helper.h"
#include "TextureUtility.h"

namespace ElysiaRenderer
{
	struct RenderTextureDesc
	{
		UINT64 Width = 0;
		UINT64 Height = 0;
		DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
		bool IsDepth = false;
		bool EnableRandomWrite = false;
		TextureDimension Dimension = TextureDimension::Tex2D;
		UINT64 MSAASamples = 1;
		UINT64 ArraySize = 1;
		UINT16 MipmapLevels = 1;
		const wchar_t* Name = nullptr;
	};


}