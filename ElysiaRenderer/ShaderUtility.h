#pragma once
#include "Helper.h"

namespace ElysiaRenderer
{
	enum ShaderQueue : UINT
	{
		Shadow = 1000,
		GBuffer = 1500,
		Opaque = 2000,
		Skybox = 3000,
		Transparent = 4000,
		Blit = 5000
	};

	enum class ShaderType : uint8_t
	{
		Vertex = 0,
		Pixel = 1,
		Compute = 2
	};

	struct ShaderCreateDesc
	{
		std::wstring shaderName;	// include file type(such as ".hlsl")
		std::wstring entryPoint;
		ShaderType shaderType;
	};

	struct ShaderVariable
	{
	public:
		enum Type : UINT64
		{
			DescriptorHeap = 0,
			ConstantBuffer,
			TypeCount
		};

		Type type;
		UINT registerPos = 0;
		UINT spaceID = 0;
	};
}