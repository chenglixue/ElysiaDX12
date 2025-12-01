#pragma once
#include "../Utility/PipelineResourceUtility.h"
#include "../Utility/ShaderUtility.h"
#include "../Utility/BufferUtility.h"

namespace ElysiaRenderer
{
	struct PipelineStateObject;
	class ShaderVariantManager;
	
	class DX12Shader
	{
	public:
		DX12Shader() = default;
		DX12Shader(std::unique_ptr<ShaderVariantManager> );
		~DX12Shader();

		void SetRenderStates(const std::unordered_map<std::wstring, std::wstring>&);

		const std::unordered_map<std::wstring, std::wstring>& GetRenderStates() const noexcept;
		ShaderVariantManager* GetVariantManager() const noexcept;
	private:
		std::unique_ptr<ShaderVariantManager> m_pShaderVariantManager = nullptr;
		std::unordered_map<std::wstring, std::wstring> m_renderStates;
	};

	
}