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
}
