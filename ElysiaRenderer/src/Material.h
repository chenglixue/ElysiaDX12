#pragma once
#include "lib/Utility/Helper.h"

#include "lib/Utility/ShaderUtility.h"
#include "Shader.h"
#include "lib/Model/ModelImporter.h"

namespace ElysiaRenderer
{
	class Shader;

	class Material
	{
	public:
		Material() = default;
		Material(std::vector<ShaderPass>& shaderPasses);
		Material(std::vector<ShaderPass>& shaderPasses, MeshRender* m_pMeshRender);
		~Material() = default;

		void Init(std::vector<ShaderPass>& shaderPasses);
		const PassData& GetPassData(UINT passIndex) const noexcept;
		const UINT FindPassIndex(const std::string& name) const noexcept;
		bool HasMeshRender() const noexcept;

		template<typename T>
		void SetConstantVariable(const std::string& name, T data, UINT passID = 0);
		template<typename T>
		void SetConstantVariable(const size_t hash, T data, UINT passID = 0);
		void ApplyConstantData();

		void SetPipelineResourceLayout(PipelineResourceLayout* pPipelineResourceLayout);

	private:
		std::mutex m_setDataMutex;
		std::vector<PassData> m_passDatas;
		MeshRender* m_pMeshRender;
		std::unique_ptr<DX12BufferResource> m_pPassConstantBuffer;
		std::unique_ptr<DX12BufferResource> m_pFrameConstantBuffer;
	};
}