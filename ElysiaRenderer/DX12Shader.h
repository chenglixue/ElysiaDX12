#pragma once
#include "stdafx.h"

namespace ElysiaRenderer
{
	enum ShaderType : uint8_t
	{
		Vertex,
		Pixel,
		Compute
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