#include "stdafx.h"

#include "RenderResource.h"

namespace ElysiaRenderer
{
	std::unique_ptr<RenderResource> g_pRenderResource = nullptr;

	RenderResource::RenderResource()
	{
		m_pCBVPassParameter = std::make_unique<CBVMainPassParameter>();
		m_perObjectBindResourceSpace = std::make_unique<PipelineResourceSpace>();
		m_perMainPassBindResourceSpace = std::make_unique<PipelineResourceSpace>();
	}

	RenderResource::~RenderResource()
	{

	}

	CBVMainPassParameter* RenderResource::GetCBVPassParameter()
	{
		return m_pCBVPassParameter.get();
	}

}