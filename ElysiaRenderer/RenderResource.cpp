#include "RenderResource.h"

namespace ElysiaRenderer
{
	RenderResource::RenderResource(DX12Device* device)
		: m_device(device)
	{
		m_pCBVPassParameter = std::make_unique<CBVMainPassParameter>();
		
		m_CBVObjectParameters = std::make_unique<CBVObjectParameter>();

	}

	RenderResource::~RenderResource()
	{

	}

	CBVObjectParameter* RenderResource::GetCBVObjectParameter()
	{
		return m_CBVObjectParameters.get();
	}

	CBVMainPassParameter* RenderResource::GetCBVPassParameter()
	{
		return m_pCBVPassParameter.get();
	}

}