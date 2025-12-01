#pragma once
#include "lib/Utility/Helper.h"

#include "lib/Utility/ShaderUtility.h"
#include "Shader.h"
#include "lib/Model/ModelImporter.h"
#include "lib/Utility/Hash.h"

namespace ElysiaRenderer
{
	class Shader;
	
	struct PassData
	{
		UINT PassIndex;
		std::string Name;
		std::unique_ptr<DX12Shader> pShader = nullptr;
		D3D12_RASTERIZER_DESC		RasterizerDesc;
		D3D12_BLEND_DESC			BlendDesc;
		D3D12_DEPTH_STENCIL_DESC	DepthStencilDesc;
		const ShaderVariantData*	pCurrVariantData = nullptr;
		const PipelineStateObject*  pPipelineStateObject = nullptr;
		
		struct SaveData
		{
			const ShaderVariantData*	pCurrVariantData = nullptr;
			const PipelineStateObject*  pPipelineStateObject = nullptr;
		};
		
		template<>
		struct hash<std::vector<std::wstring>>
		{
			using argument_type = std::vector<std::wstring>;
			using result_type = size_t;

			size_t operator()(argument_type const& v) const
			{
				return xxh::GetHash(v);
			}
		};
		template<>
		struct equal_to<std::vector<std::wstring>> 
		{
			using argument_type = std::vector<std::wstring>;
			using result_type = size_t;

			bool operator()(argument_type const& a, argument_type const& b) const 
			{
				return memcmp(&a, &b, sizeof(argument_type)) == 0;
			}
		};
		
		
		// enableKeywords : SaveData
		std::unordered_map<std::vector<std::wstring>, SaveData> keywords;
	};
	
	

	class Material
	{
	public:
		Material() = default;
		Material(std::vector<ShaderPass>& shaderPasses);
		Material(std::vector<ShaderPass>& shaderPasses, MeshRender* m_pMeshRender);
		~Material() = default;

		void Init(std::vector<ShaderPass>& shaderPasses);
		PassData& GetPassData(UINT passIndex) noexcept;
		const UINT FindPassIndex(const std::string& name) const noexcept;
		bool HasMeshRender() const noexcept;

		void SetPipelineResourceLayout(PipelineResourceLayout* pPipelineResourceLayout);
		void SetCurrVariantData(const ShaderVariantData* pCurrVariantData);
		const ShaderVariantData* GetCurrVariantData() const;
		

	private:
		std::mutex m_setDataMutex;
		std::vector<PassData> m_passDatas;
		MeshRender* m_pMeshRender;
		const ShaderVariantData* m_pCurrVariantData;
		std::unique_ptr<DX12BufferResource> m_pPassConstantBuffer = nullptr;
		std::unique_ptr<DX12BufferResource> m_pFrameConstantBuffer = nullptr;
	};
}