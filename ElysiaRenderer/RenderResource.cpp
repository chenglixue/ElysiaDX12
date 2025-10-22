#include "RenderResource.h"

namespace ElysiaRenderer
{
	std::unique_ptr<RenderResource> RenderResource::m_instance;
	std::once_flag RenderResource::m_initInstanceFlag;

	std::unique_ptr<PipelineResourceSpace> RenderResource::m_perObjectBindResourceSpace = std::make_unique<PipelineResourceSpace>();
	std::unique_ptr<PipelineResourceSpace> RenderResource::m_perMainPassBindResourceSpace = std::make_unique<PipelineResourceSpace>();

	RenderResource::RenderResource()
	{
		m_pCBVPassParameter = std::make_unique<CBVMainPassParameter>();
		
	}

	RenderResource::~RenderResource()
	{

	}

	CBVMainPassParameter* RenderResource::GetCBVPassParameter()
	{
		return m_pCBVPassParameter.get();
	}

}