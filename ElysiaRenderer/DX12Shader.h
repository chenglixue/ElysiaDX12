#pragma once
#include "stdafx.h"

namespace ElysiaRenderer
{
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

	class DX12Shader
	{
	public:
		DX12Shader();
		DX12Shader(ID3DBlob* shader);
		~DX12Shader();

		ID3DBlob* GetShader() const
		{
			return m_shader;
		}

	private:
		ID3DBlob* m_shader;
	};
}