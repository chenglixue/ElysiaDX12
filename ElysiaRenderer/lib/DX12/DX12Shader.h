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
		DX12Shader();
		DX12Shader(CComPtr<IDxcBlob> shader);
		DX12Shader(std::unique_ptr<ShaderVariantManager> );
		~DX12Shader();

		CComPtr<IDxcBlob>& GetShader();
		const std::vector<ShaderVariantData>& GetShaderVariantDatas();
	private:
		CComPtr<IDxcBlob> m_shader;

		std::unique_ptr<ShaderVariantManager> m_pShaderVariantManager = nullptr;
	};

	struct PassData
	{
		UINT PassIndex;
		std::string Name;
		std::unique_ptr<DX12Shader> pVSShader = nullptr;
		std::unique_ptr<DX12Shader>	pPSShader = nullptr;
		std::unique_ptr<DX12Shader>	pCSShader = nullptr;
		D3D12_RASTERIZER_DESC		RasterizerDesc;
		D3D12_BLEND_DESC			BlendDesc;
		D3D12_DEPTH_STENCIL_DESC	DepthStencilDesc;
		std::unique_ptr<PipelineResourceLayout> MeshResourceLayouts;
		BufferCreationDesc			ObjectBufferDesc;
		BufferCreationDesc			MaterialBufferDesc;
	};
}