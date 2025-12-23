#pragma once
#include "PipelineResourceUtility.h"
#include "ShaderUtility.h"
#include "BufferUtility.h"

namespace ElysiaCore
{
	struct PipelineStateObject;
	class ShaderVariantManager;
	
	class DX12Shader
	{
	public:
		DX12Shader() = default;
		DX12Shader(std::unique_ptr<ShaderVariantManager>,  std::unique_ptr<ShaderKeywordSpace> pKeywordSpace);
		~DX12Shader();

		void SetRenderStates(const std::unordered_map<std::wstring, std::wstring>&);
		const std::unordered_map<std::wstring, std::wstring>& GetRenderStates() const noexcept;
		ShaderVariantManager* GetVariantManager() const noexcept;

		void BakeVertexLayout();
	private:
		std::unique_ptr<ShaderKeywordSpace> m_pKeywordSpace = nullptr;
		std::unique_ptr<ShaderVariantManager> m_pShaderVariantManager = nullptr;
		std::unordered_map<std::wstring, std::wstring> m_renderStates;

		struct StableVertexLayout
		{
			std::vector<D3D12_INPUT_ELEMENT_DESC> m_vertexInputLayoutElementDescs;
			std::vector <std::string> m_vertexInputElementSemanticNames;
		};
		std::vector<StableVertexLayout> m_stableVertexLayout;
	};

	
}