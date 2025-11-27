#pragma once
#include "Helper.h"
#include "PipelineResourceUtility.h"

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
		ShaderConstantVariableDesc() = default;
		~ShaderConstantVariableDesc()
		{
			
		}

		// UINT		PassID = -1;
		UINT		SpaceID = 0;
		UINT        StartOffset;    // Offset in constant buffer's backing store
		UINT        Size;           // Size of variable (in bytes)
		bool		IsDirty = false;
		std::vector<char> 	pData;
	};

	struct ShaderPass
	{
		std::string					Name;
		std::wstring				FilePath;
		bool						IsComputeShader = false;
		std::wstring				VertexEntryPoint = L"VS";
		std::wstring				FragmentEntryPoint = L"PS";
		std::wstring				ComputeEntryPoint = L"CS";
		D3D12_RASTERIZER_DESC		RasterizerDesc;
		D3D12_BLEND_DESC			BlendDesc;
		D3D12_DEPTH_STENCIL_DESC	DepthStencilDesc;
	};

	

	DXGI_FORMAT MaskToFormat(const uint32_t Mask);

}