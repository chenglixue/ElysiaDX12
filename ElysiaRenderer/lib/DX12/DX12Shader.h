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

	struct PassData
	{
		UINT PassIndex;
		std::string Name;
		std::unique_ptr<DX12Shader> pShader = nullptr;
		D3D12_RASTERIZER_DESC		RasterizerDesc;
		D3D12_BLEND_DESC			BlendDesc;
		D3D12_DEPTH_STENCIL_DESC	DepthStencilDesc;
		std::unique_ptr<PipelineResourceLayout> MeshResourceLayouts;
	};
}