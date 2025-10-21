#include "RenderResource.h"

namespace ElysiaRenderer
{
	std::unique_ptr<RenderResource> m_instance;
	std::once_flag m_initInstanceFlag;

	std::unique_ptr<PipelineResourceSpace> RenderResource::m_perObjectBindResourceSpace = nullptr;
	std::unique_ptr<PipelineResourceSpace> RenderResource::m_perMainPassBindResourceSpace = nullptr;

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