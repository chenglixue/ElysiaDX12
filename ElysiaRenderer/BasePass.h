#pragma once
#include "ModelImporter.h"
#include "DX12Shader.h"
#include "RenderPassData.h"
#include "UserData.h"

namespace ElysiaRenderer
{
	using namespace ElysiaModel;

	class RenderTexture;
	class DX12GraphicsContext;

	class BasePass
	{
	public:
		BasePass();
		virtual ~BasePass();

		virtual void Setup(const RenderPassData& renderPassData);
		virtual void Configure() = 0;
		virtual void Execute() = 0;
		virtual void Render() = 0;

		virtual void Dispose();

		void SetConstantData(const std::string& name, const void* pData);
		void ApplyConstantData();

	protected:
		UINT2 m_renderSize;
		DX12GraphicsContext* m_pCommand = nullptr;
		std::unordered_map<UINT, std::unique_ptr<PipelineStateObject>> m_pGraphicsPipelineStates;

		std::unordered_map<std::string, ShaderVariable> m_shaderVariables;
		std::unordered_map<std::string, ShaderConstantVariableDesc> m_constantVariableDescs;
		PipelineResourceLayout m_meshResourceLayout;
	};
}