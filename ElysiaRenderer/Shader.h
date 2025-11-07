#pragma once
#include "Helper.h"
#include "MObject.h"

#include "ShaderUtility.h"

namespace ElysiaRenderer
{
	class PipelineResourceLayout;
	class PipelineStateObject;

	class Shader : public MObject
	{
	public:
		Shader() = default;
		Shader(std::vector<ShaderPass>& shaderPasses);
		~Shader() = default;

	private:
		std::unordered_map<std::string, UINT> m_shaderPassIDs;
		std::unordered_map<std::string, ShaderVariable> m_shaderVariables;
		std::unordered_map<std::string, ShaderConstantVariableDesc> m_constantVariableDescs;
		std::unordered_map<std::string, std::unique_ptr<PipelineResourceLayout>> m_meshResourceLayouts;
	};
}