#pragma once
#include "lib/Utility/Helper.h"

#include "lib/Utility/ShaderUtility.h"
#include "lib/Model/ModelImporter.h"
#include "lib/Utility/Hash.h"
#include "lib/DX12/DX12Shader.h"

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

	struct RuntimeCBuffer
	{
		UINT8* GPUPtr = nullptr;
		std::vector<UINT8> CPUPtr{};

		UINT32 DirtyBegin = UINT32_MAX;
		UINT32 DirtyEnd = 0;

		void MakeDirty(UINT32 offset, UINT32 size)
		{
			if (offset < DirtyEnd) 
			{
				// �������ݸ��������������򣬸��½���λ��
				DirtyEnd = max(DirtyEnd, offset + size);
			}
			else 
			{
				// ������������Ŀ�ʼ�ͽ���λ��
				DirtyBegin = min(DirtyBegin, offset);
				DirtyEnd = max(DirtyEnd, offset + size);
			}
		}

		bool HasDirtyRange() const noexcept
		{
			return DirtyBegin < DirtyEnd;
		}

		void ClearDirty()
		{
			
			DirtyBegin = UINT32_MAX;
			DirtyEnd = 0;
		}
	};
	struct MaterialRuntimeCBuffer
	{
		std::array<RuntimeCBuffer, NUM_RESOURCE_SPACES> CBuffers;
	};
	
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
		std::unique_ptr<MaterialRuntimeCBuffer> pMaterialCBuffer = nullptr;
		UINT8* pPassGPUPtr = nullptr;
		UINT8* pFrameGPUPtr = nullptr;
		
		struct SaveData
		{
			ShaderVariantData*	pCurrVariantData = nullptr;
			PipelineStateObject*  pPipelineStateObject = nullptr;
			UINT8* pPassGPUPtr = nullptr;
			UINT8* pFrameGPUPtr = nullptr;
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
		Material(DX12Device* pDevice,std::vector<ShaderPass>& shaderPasses, MeshRender* m_pMeshRender);
		~Material() = default;

		void Init(std::vector<ShaderPass>& shaderPasses);
		PassData& GetPassData(UINT passIndex) noexcept;
		const UINT FindPassIndex(const std::string& name) const noexcept;
		bool HasMeshRender() const noexcept;
		void CreateMaterialCBuffer(size_t passIndex);
		
		void SetMaterialCBufferGPUPtr(UINT spaceID, size_t passIndex = 0);

		void SetFloat(const size_t hashName, const float newValue, size_t passIndex = 0);
		void SetInt(const size_t hashName, const int newValue, size_t passIndex = 0);
		void SetUINT(const size_t hashName, const UINT newValue, size_t passIndex = 0);
		void SetBool(const size_t hashName, const int newValue, size_t passIndex = 0);
		void SetMatrix(const size_t hashName, const Matrix newValue, size_t passIndex = 0);
		void SetFloatArray(const size_t hashName, const std::vector<float> newValue, size_t passIndex = 0);
		void SetVector2Array(const size_t hashName, const std::vector<Vector2> newValue, size_t passIndex = 0);
		void SetVector3Array(const size_t hashName, const std::vector<Vector3> newValue, size_t passIndex = 0);
		void SetVector4Array(const size_t hashName, const std::vector<Vector4> newValue, size_t passIndex = 0);
		void Flush();

	private:
		std::mutex m_setDataMutex;
		std::vector<PassData> m_passDatas;
		DX12Device* m_pDevice = nullptr;
		MeshRender* m_pMeshRender = nullptr;

		template<typename T>
		void UpdateCBuffer(RuntimeCBuffer& CBuffer, UINT32 offset, const T data);
		template<typename T>
		void UpdateCBuffer(RuntimeCBuffer& CBuffer, UINT32 offset, const std::vector<T> data);
	};
}