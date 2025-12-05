#include "stdafx.h"

#include "RenderResource.h"

namespace ElysiaRenderer
{
	std::unique_ptr<RenderResource> RenderResource::m_instance;
	std::once_flag RenderResource::m_initInstanceFlag;

	RenderResource::RenderResource() :
		m_perFrameBindResourceSpace(std::make_unique<PipelineResourceSpace>()),
		m_pCBVFrameVariable(std::make_unique<CBVFrameVariable>())
	{
	}

	RenderResource::~RenderResource()
	{

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

	void RenderResource::SetDisplayMode(CAULDRON_DX12::DisplayMode newDisplayMode)
	{
		m_currDisplayMode = newDisplayMode;
	}

	size_t PropertyToID(const std::string& name)
	{
		auto hash = xxh::GetHash(name);

		return hash;
	}
}