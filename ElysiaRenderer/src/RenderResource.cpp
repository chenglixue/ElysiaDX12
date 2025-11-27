#include "stdafx.h"

#include "RenderResource.h"

namespace ElysiaRenderer
{
	std::unique_ptr<RenderResource> g_pRenderResource = nullptr;
	std::unordered_map<size_t, std::string> g_shaderConstantVariables{};

	RenderResource::RenderResource() :
		m_perObjectBindResourceSpace(std::make_unique<PipelineResourceSpace>()),
		m_perFrameBindResourceSpace(std::make_unique<PipelineResourceSpace>()),
		m_pCBVFrameVariable(std::make_unique<CBVFrameVariable>())
	{
	}

	RenderResource::~RenderResource()
	{

	}

	PipelineResourceSpace* RenderResource::GetPerObjectBindResourceSpace()
	{
		return m_perObjectBindResourceSpace.get();
	}
	
	PipelineResourceSpace* RenderResource::GetPerFrameBindResourceSpace()
	{
		return m_perFrameBindResourceSpace.get();
	}
	
	CBVFrameVariable* RenderResource::GetCBVFrameVariable()
	{
		return m_pCBVFrameVariable.get();
	}

	CAULDRON_DX12::DisplayMode RenderResource::GetDisplayMode() const noexcept
	{
		return m_currDisplayMode;
	}

	std::string RenderResource::GetShaderConstantVariable(size_t hash) const noexcept
	{
		return g_shaderConstantVariables.at(hash);
	}

	void RenderResource::SetDisplayMode(CAULDRON_DX12::DisplayMode newDisplayMode)
	{
		m_currDisplayMode = newDisplayMode;
	}

	void AddShaderConstantVariable(const size_t hash, const std::string& name)
	{
		auto result = g_shaderConstantVariables.try_emplace(hash);
		if (result.second)
		{
			result.first->second = name;
		}
	}

	size_t PropertyToID(const std::string& name)
	{
		auto hash = xxh::GetHash(name);

		AddShaderConstantVariable(hash, name);

		return hash;
	}
}