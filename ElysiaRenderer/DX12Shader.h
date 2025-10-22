#pragma once
#include "stdafx.h"
#include <dxcapi.h>         // Be sure to link with dxcompiler.lib.
#include <d3d12shader.h>    // Shader reflection.

namespace ElysiaRenderer
{
	enum ShaderQueue : UINT
	{
		Shadow = 1000,
		Opaque = 2000,
		Skybox = 3000,
		Transparent = 4000
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

	class DX12Shader
	{
	public:
		DX12Shader();
		DX12Shader(CComPtr<IDxcBlob> shader);
		~DX12Shader();

		CComPtr<IDxcBlob>& GetShader()
		{
			return m_shader;
		}

	private:
		CComPtr<IDxcBlob> m_shader;
	};

	extern std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>> g_vertexShaders;
	extern std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>> g_pixelShaders;
	extern std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>>  g_computeShaders;

	inline std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>>& GetVertexShaders()
	{
		return g_vertexShaders;
	}
	inline std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>>& GetPixelShaders()
	{
		return g_pixelShaders;
	}
	inline std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>>& GetComputeShaders()
	{
		return g_computeShaders;
	}
}