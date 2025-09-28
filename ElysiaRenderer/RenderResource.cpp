#include "RenderResource.h"

namespace ElysiaRenderer
{
	RenderResource::RenderResource(DX12Device* device)
		: m_device(device)
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