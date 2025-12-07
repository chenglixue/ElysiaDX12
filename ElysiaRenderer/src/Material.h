#pragma once
#include "lib/Utility/Helper.h"

#include "lib/Utility/ShaderUtility.h"
#include "lib/Model/ModelImporter.h"
#include "lib/Utility/Hash.h"
#include "lib/DX12/DX12Shader.h"
#include "MaterialParams.h"

namespace std
{
	template<>
	struct std::hash<std::vector<std::wstring>>
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
}

namespace ElysiaRenderer
{
	class Shader;
	class UploadRingBuffer;
	
	struct PassData
	{
		UINT PassIndex;
		std::string Name;
		std::unique_ptr<DX12Shader> pShader = nullptr;
		D3D12_RASTERIZER_DESC		RasterizerDesc;
		D3D12_BLEND_DESC			BlendDesc;
		D3D12_DEPTH_STENCIL_DESC	DepthStencilDesc;
		ShaderVariantData*	pCurrVariantData = nullptr;
		PipelineStateObject*  pPipelineStateObject = nullptr;
		
		struct SaveData
		{
			ShaderVariantData*	pCurrVariantData = nullptr;
			PipelineStateObject*  pPipelineStateObject = nullptr;
		};
		
		// enableKeywords : SaveData
		std::unordered_map<std::vector<std::wstring>, SaveData> keywords;
		
		PipelineResourceLayout* GetMeshResourceLayout()
		{
			assert(pCurrVariantData);
			return pCurrVariantData->pMeshResourceLayout.get();
		}
	};
	
	class Material
	{
	public:
		Material() = default;
		Material(DX12Device* pDevice, std::vector<ShaderPass>& shaderPasses);
		~Material() = default;

		void Init(std::vector<ShaderPass>& shaderPasses);
		PassData& GetPassData(UINT passIndex) noexcept;
		const UINT FindPassIndex(const std::string& name) const noexcept;
		MaterialParameterBlock& GetParameterBlock() {return m_parameterBlock;}

		void SetInt(size_t nameHash, int v);
		void SetUInt(size_t nameHash, unsigned int v);
		void SetFloat(size_t nameHash, float v);
		void SetFloat2(size_t nameHash, const Vector2& v);
		void SetFloat3(size_t nameHash, const Vector3& v);
		void SetFloat4(size_t nameHash, const Vector4& v);
		void SetMatrix(size_t nameHash, const Matrix& m);
		void SetFloatArray(size_t nameHash, const std::vector<float>& values);
		void SetIntArray(size_t nameHash, const std::vector<int>& values);
		void SetUINTArray(size_t nameHash, const std::vector<UINT>& values);
		void SetVector2Array(size_t nameHash, const std::vector<Vector2>& values);
		void SetVector3Array(size_t nameHash, const std::vector<Vector3>& values);
		void SetVector4Array(size_t nameHash, const std::vector<Vector4>& values);
		void SetMatrixArray(size_t nameHash, const std::vector<Matrix>& values);

	private:
		std::mutex m_setDataMutex;
		std::vector<PassData> m_passDatas;
		DX12Device* m_pDevice = nullptr;
		MaterialParameterBlock m_parameterBlock;	// 所有材质参数
	};
}