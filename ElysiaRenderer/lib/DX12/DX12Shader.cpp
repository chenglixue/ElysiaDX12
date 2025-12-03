#include "stdafx.h"
#include "DX12Shader.h"
#include "DX12PipelineState.h"
#include <d3d12shader.h>    // Shader reflection.
#include "../Utility/ShaderCompileOptions.h"
#include "Manager/ShaderVariantManager.h"

namespace ElysiaRenderer
{
	DX12Shader::DX12Shader(std::unique_ptr<ShaderVariantManager> pShaderVariantManager, std::unique_ptr<ShaderKeywordSpace> pKeywordSpace) :
		m_pShaderVariantManager(std::move(pShaderVariantManager)),
		m_pKeywordSpace(std::move(pKeywordSpace))
	{
	}

	DX12Shader::~DX12Shader()
	{
		
	}

	ShaderVariantManager* DX12Shader::GetVariantManager() const noexcept
	{
		return m_pShaderVariantManager.get();
	}
	const std::unordered_map<std::wstring, std::wstring>& DX12Shader::GetRenderStates() const noexcept
	{
		return m_renderStates;
	}
	void DX12Shader::SetRenderStates(const std::unordered_map<std::wstring, std::wstring>& renderStates)
	{
		m_renderStates = renderStates;
	}

	void DX12Shader::BakeVertexLayout()
	{
		auto allVariants = GetVariantManager()->GetAllVariants();
		
		m_stableVertexLayout.clear();
		m_stableVertexLayout.resize(allVariants.size());

		for (size_t variantIndex = 0; variantIndex < allVariants.size(); ++variantIndex)
		{
			auto& currVariant = allVariants[variantIndex];
			auto& currReflectionData = currVariant.StageShaders.at(ShaderType::Vertex).ReflectionData;
			auto& currVertexLayout = m_stableVertexLayout[variantIndex];

			currVertexLayout.m_vertexInputElementSemanticNames.clear();
			currVertexLayout.m_vertexInputElementSemanticNames.reserve(currReflectionData.InputElementSemanticNames.size());

			for (const auto& name : currReflectionData.InputElementSemanticNames)
			{
				currVertexLayout.m_vertexInputElementSemanticNames.emplace_back(name);
			}

			currVertexLayout.m_vertexInputLayoutElementDescs.clear();
			currVertexLayout.m_vertexInputLayoutElementDescs.reserve(currReflectionData.InputLayoutElementDescs.size());

			for (size_t descIndex = 0; descIndex < currReflectionData.InputLayoutElementDescs.size(); ++descIndex)
			{
				auto& currElement = currReflectionData.InputLayoutElementDescs[descIndex];

				const char* semanticPtr = nullptr;
				// If original element has a semantic name index, prefer that; otherwise use same index
				// We assume var.InputElementSemanticNames aligns with InputLayoutElementDescs indexes.
				semanticPtr = descIndex < currVertexLayout.m_vertexInputElementSemanticNames.size() ?
					currVertexLayout.m_vertexInputElementSemanticNames[descIndex].c_str() : currElement.SemanticName;

				currElement.SemanticName = semanticPtr;
				currVertexLayout.m_vertexInputLayoutElementDescs.emplace_back(currElement);

#ifdef DEBUG
				std::cout << "Input Element name is " << currVertexLayout.m_vertexInputLayoutElementDescs.back().SemanticName << std::endl;
				std::cout << "Input Element Index is " << currVertexLayout.m_vertexInputLayoutElementDescs.back().SemanticIndex << std::endl;
				std::cout << "Input Element Format is " << DXGIFormatToString(currVertexLayout.m_vertexInputLayoutElementDescs.back().Format) << std::endl;
				std::cout << std::endl;
#endif // DEBUG
			}

			if (!currVertexLayout.m_vertexInputLayoutElementDescs.empty())
			{
				currReflectionData.InputLayoutElementDescs = currVertexLayout.m_vertexInputLayoutElementDescs;
				currReflectionData.InputLayoutDesc.pInputElementDescs = currReflectionData.InputLayoutElementDescs.data();
				currReflectionData.InputLayoutDesc.NumElements = static_cast<UINT>(currReflectionData.InputLayoutElementDescs.size());
				currReflectionData.InputLayoutDesc.pInputElementDescs = currVertexLayout.m_vertexInputLayoutElementDescs.data();
			}
			else
			{
				currReflectionData.InputLayoutDesc.pInputElementDescs = nullptr;
				currReflectionData.InputLayoutDesc.NumElements = 0;
			}

		}
	}
}
