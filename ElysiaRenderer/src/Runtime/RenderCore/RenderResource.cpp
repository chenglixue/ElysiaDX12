#include "stdafx.h"
#include "RenderResource.h"

#include "Runtime/Core/PipelineResourceSpace.h"
#include "Runtime/Core/DX12Device.h"

namespace ElysiaRenderer
{
	std::unique_ptr<RenderResource> RenderResource::m_instance;
	std::once_flag RenderResource::m_initInstanceFlag;

	RenderResource::RenderResource() :
		m_perFrameBindResourceSpaces(),
		m_CBVFrameVariable()
	{
		for (UINT i = 0; i < ElysiaHelper::NUM_FRAMES_IN_FLIGHT; ++i)
		{
			m_perFrameBindResourceSpaces[i] = std::make_unique<PipelineResourceSpace>();
		}
	}

	RenderResource::~RenderResource()
	{

	}

	PipelineResourceSpace* RenderResource::GetPerFrameBindResourceSpace(UINT frameID)
	{
		return m_perFrameBindResourceSpaces[frameID].get();
	}
	
	CBVFrameVariable& RenderResource::GetCBVFrameVariable()
	{
		return m_CBVFrameVariable;
	}

	CAULDRON_DX12::DisplayMode RenderResource::GetDisplayMode() const noexcept
	{
		return m_currDisplayMode;
	}

	void RenderResource::SetDisplayMode(CAULDRON_DX12::DisplayMode newDisplayMode)
	{
		m_currDisplayMode = newDisplayMode;
	}
	
	size_t RenderResource::AddPropertyID(const std::string& name)
	{
		auto nameHash = xxh::GetHash(name);
		auto emplaceResult = m_nameHashs.try_emplace(nameHash);
		if(emplaceResult.second)
		{
			emplaceResult.first->second = name;
		}
		return nameHash;
	}
	
	std::string RenderResource::GetPropertyName(size_t hash) const noexcept
	{
		if(m_nameHashs.contains(hash))
		{
			return m_nameHashs.at(hash);
		}
		
		return "";
	}

	size_t PropertyToID(const std::string& name)
	{
		return RenderResource::GetInstance().AddPropertyID(name);
	}
}
