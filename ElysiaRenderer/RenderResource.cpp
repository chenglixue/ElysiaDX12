#include "RenderResource.h"

namespace ElysiaRenderer
{
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