#pragma once
#include "Helper.h"


namespace ElysiaRenderer
{
	struct PipelineResourceLayout;

	class DX12Shader;

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
		std::string name;
		UINT size = 0;
	};

	struct ShaderConstantVariableDesc
	{
		~ShaderConstantVariableDesc()
		{
			if (!pData)
				return;
			delete[] pData;
			pData = NULL;
		}

		UINT					SpaceID = 0;
		UINT                    StartOffset;    // Offset in constant buffer's backing store
		UINT                    Size;           // Size of variable (in bytes)
		void*					pData;
	};

	struct ShaderPass
	{
		std::string					Name;
		std::wstring				FilePath;
		std::wstring				VertexEntryPoint = L"VS";
		std::wstring				FragmentEntryPoint = L"PS";
		D3D12_RASTERIZER_DESC		RasterizerDesc;
		D3D12_BLEND_DESC			BlendDesc;
		D3D12_DEPTH_STENCIL_DESC	DepthStencilDesc;
	};

	struct PassData
	{
		UINT PassIndex;
		std::unique_ptr<DX12Shader> pVSShader = nullptr;
		std::unique_ptr<DX12Shader>	pPSShader = nullptr;
		D3D12_RASTERIZER_DESC		RasterizerDesc;
		D3D12_BLEND_DESC			BlendDesc;
		D3D12_DEPTH_STENCIL_DESC	DepthStencilDesc;
		std::unique_ptr<PipelineResourceLayout> MeshResourceLayouts{};
	};

	DXGI_FORMAT MaskToFormat(const uint32_t Mask);
}