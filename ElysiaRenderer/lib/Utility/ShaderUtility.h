#pragma once
#include "Helper.h"
#include "PipelineResourceUtility.h"
#include <regex>
#include "ShaderCompileOptions.h"
#include "lib/Utility/ShaderKeywordSet.h"

namespace ElysiaRenderer
{
	enum class ShaderType : UINT
	{
		Vertex = 0,
		Pixel = 1,
		Compute = 2
	};

	struct ShaderStageDesc
	{
		ShaderType ShaderType;
		std::wstring ShaderName;	// include file type(such as ".hlsl")
		std::wstring EntryPoint;
	};

	struct ShaderCreateDesc
	{
		std::vector<ShaderStageDesc> stages;
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

	struct PragmaKeywordGroup
	{
		std::vector<std::wstring> Keywords;
	};

	struct ShaderPragmaInfo
	{
		std::vector<PragmaKeywordGroup> KeywordGroups;
	};

	struct ShaderReflectionData
	{
		struct ShaderConstantVariableDesc
		{
			UINT		SpaceID = 0;
			UINT        StartOffset;    // Offset in constant buffer's backing store
			UINT        Size;           // Size of variable (in bytes)
			std::string	Name;
		};

		struct ShaderVariable
		{
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
			std::vector<ShaderConstantVariableDesc> members;
		};
		
		std::unordered_map<std::string, ShaderVariable> cbuffers;

		D3D12_INPUT_LAYOUT_DESC InputLayoutDesc;
		std::vector<D3D12_INPUT_ELEMENT_DESC> InputLayoutElementDescs;
		std::vector <std::string> InputElementSemanticNames;

		void Merge(const ShaderReflectionData& data)
		{
			for(auto& cbuffer : data.cbuffers)
			{
				auto emplaceResult = cbuffers.try_emplace(cbuffer.first);
				if(emplaceResult.second)
				{
					emplaceResult.first->second = cbuffer.second;
				}
			}
		}
	};

	struct ShaderBytecode
	{
		CComPtr<IDxcBlob> bytecode;
		std::wstring entry;
		std::wstring target;
		ShaderReflectionData ReflectionData;
	};

	struct ShaderVariantData
	{
		ShaderKeywordSet KeywordSet;
		
		std::unordered_map<ShaderType, ShaderBytecode> StageShaders;
		ShaderReflectionData MergedReflectionData;
		std::unique_ptr<PipelineResourceLayout> MeshResourceLayouts;
		uint8_t* pMergedCBuffer = nullptr;
	};
	
	struct MergeCBufferLayout
	{
		size_t totalSize = 0;
		struct CBuffer
		{
			size_t offset;       // global offset
			size_t size;
			size_t hashName;       // name hash
			uint32_t space;      // register space
		};
		
		std::unordered_map<uint32_t, CBuffer> variables;
	};

	DXGI_FORMAT MaskToFormat(const uint32_t Mask);

	static bool IsUnderlineKeyword(const std::wstring& s);

	std::unordered_map<std::wstring, std::wstring> ParseShaderRenderPragmas(const std::wstring& source);
	
	ShaderPragmaInfo ParseShaderPragmas(const std::wstring& source);

	// ShaderCompileOptions BuildOptionsForVariant(
	// const ShaderCompileOptions& base,
	// const ShaderVariant& variant);
}