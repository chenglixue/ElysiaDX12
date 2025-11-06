#pragma once
#include "stdafx.h"
#include "DX12Device.h"
#include "CBVParameter.h"

namespace ElysiaRenderer
{
	class RenderResource
	{
	public:
		RenderResource();
		RenderResource(const RenderResource& rhs) = delete;
		RenderResource& operator=(RenderResource& rhs) = delete;
		RenderResource(RenderResource&& rhs) = default;
		~RenderResource();

		PipelineResourceSpace* GetPerObjectBindResourceSpace()
		{
			return m_perObjectBindResourceSpace.get();
		}
		PipelineResourceSpace* GetPerMainBindResourceSpace()
		{
			return m_perMainPassBindResourceSpace.get();
		}

		CBVMainPassParameter* GetCBVPassParameter();

	private:

		std::unique_ptr<PipelineResourceSpace> m_perObjectBindResourceSpace;
		std::unique_ptr<PipelineResourceSpace> m_perMainPassBindResourceSpace;

		DX12Device* m_device = nullptr;
		std::unique_ptr<CBVMainPassParameter> m_pCBVPassParameter = nullptr;
	};


	extern std::unique_ptr<RenderResource> g_pRenderResource;
	inline RenderResource* GetRenderResource()
	{
		if (g_pRenderResource == nullptr)
		{
			ThrowRuntimeError("Null Render Resource");
		}
		return g_pRenderResource.get();
	}
}