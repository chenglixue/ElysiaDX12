#include "RenderResource.h"

namespace ElysiaRenderer
{
	RenderResource::RenderResource(DX12Device* device)
		: m_device(device)
	{
		m_pCBVPassParameter = std::make_unique<CBVMainPassParameter>();
		
		for (size_t i = 0; i < m_CBVObjectParameters.size(); ++i)
		{
			m_CBVObjectParameters[i] = std::move(std::make_unique<CBVObjectParameter>());
		}
	}

	RenderResource::~RenderResource()
	{

	}

	CBVObjectParameter* RenderResource::GetCBVObjectParameter(UINT frameID)
	{
		return m_CBVObjectParameters[frameID].get();
	}

	CBVMainPassParameter* RenderResource::GetCBVPassParameter()
	{
		return m_pCBVPassParameter.get();
	}

}