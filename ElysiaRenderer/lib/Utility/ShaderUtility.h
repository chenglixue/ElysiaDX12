#pragma once
#include "Helper.h"
#include "PipelineResourceUtility.h"
#include <regex>
#include "ShaderCompileOptions.h"
#include "lib/Utility/ShaderKeywordSet.h"
#include <d3d12shader.h>    // Shader reflection.

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
			size_t		Name;
		};

		struct ShaderVariable
		{
			_D3D_SHADER_INPUT_TYPE type;
			UINT bindPoint = 0;
			UINT spaceID = 0;
			std::string name;
			UINT size = 0;
			std::unordered_map<size_t, ShaderConstantVariableDesc> members;
		};
		
		std::unordered_map<UINT32, ShaderVariable> cbuffers;
		D3D12_INPUT_LAYOUT_DESC InputLayoutDesc;
		std::vector<D3D12_INPUT_ELEMENT_DESC> InputLayoutElementDescs;
		std::vector <std::string> InputElementSemanticNames;

		ShaderVariable GetCBuffer(UINT32 spaceID) const
		{
			return cbuffers.at(spaceID);
		}
		
		void Merge(ShaderReflectionData& data)
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
		bool HasCBufferMember(UINT32 spaceID, const size_t hashName) const
		{
			return cbuffers.at(spaceID).members.contains(hashName);
		}
		ShaderConstantVariableDesc FindCBufferMember(UINT32 spaceID, const size_t hashName) const
		{
			return cbuffers.at(spaceID).members.at(hashName);
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
		std::unique_ptr<PipelineResourceLayout> pMeshResourceLayout;

		ShaderVariantData() =default;

		ShaderVariantData(const ShaderVariantData& other)
		: KeywordSet(other.KeywordSet),
		  StageShaders(other.StageShaders),
		  MergedReflectionData(other.MergedReflectionData),
		  pMeshResourceLayout(other.pMeshResourceLayout ? std::make_unique<PipelineResourceLayout>(*other.pMeshResourceLayout) : nullptr) {}

		// 移动构造函数
		ShaderVariantData(ShaderVariantData&& other) noexcept
			: KeywordSet(std::move(other.KeywordSet)),
			  StageShaders(std::move(other.StageShaders)),
			  MergedReflectionData(std::move(other.MergedReflectionData)),
			  pMeshResourceLayout(std::move(other.pMeshResourceLayout)) {}

		ShaderVariantData& operator=(const ShaderVariantData& rhs)
		{
			if (this != &rhs) {
				KeywordSet = rhs.KeywordSet;
				StageShaders = rhs.StageShaders;
				MergedReflectionData = rhs.MergedReflectionData;
				pMeshResourceLayout = rhs.pMeshResourceLayout ? std::make_unique<PipelineResourceLayout>(*rhs.pMeshResourceLayout) : nullptr;
			}
			return *this;
		}

		ShaderVariantData& operator=(ShaderVariantData&& rhs)
		{
			if (this != & rhs)
			{
				this->pMeshResourceLayout = std::move(rhs.pMeshResourceLayout);
				this->KeywordSet = std::move(rhs.KeywordSet);
				this->StageShaders = std::move(rhs.StageShaders);
				this->MergedReflectionData = std::move(rhs.MergedReflectionData);
			}

			return *this;
		}
	};

	DXGI_FORMAT MaskToFormat(const uint32_t Mask);

	static bool IsUnderlineKeyword(const std::wstring& s);

	std::unordered_map<std::wstring, std::wstring> ParseShaderRenderPragmas(const std::wstring& source);
	
	ShaderPragmaInfo ParseShaderPragmas(const std::wstring& source);

	// ShaderCompileOptions BuildOptionsForVariant(
	// const ShaderCompileOptions& base,
	// const ShaderVariant& variant);
}