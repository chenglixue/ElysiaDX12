#include "stdafx.h"

#include "RenderResource.h"

namespace ElysiaRenderer
{
	std::unique_ptr<RenderResource> g_pRenderResource = nullptr;

	RenderResource::RenderResource() :
		m_perObjectBindResourceSpace(std::make_unique<PipelineResourceSpace>()),
		m_pCBVFrameVariable(std::make_unique<CBVFrameVariable>())
	{
	}

	RenderResource::~RenderResource()
	{

	}

	CBVFrameVariable* RenderResource::GetCBVFrameVariable()
	{
		return m_pCBVFrameVariable.get();
	}

}