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
		PipelineResourceSpace* GetPerFrameBindResourceSpace()
		{
			return m_perFrameBindResourceSpace.get();
		}
		CBVFrameVariable* GetCBVFrameVariable();

	private:

		std::unique_ptr<PipelineResourceSpace> m_perObjectBindResourceSpace;
		std::unique_ptr<PipelineResourceSpace> m_perFrameBindResourceSpace;

		DX12Device* m_device = nullptr;
		std::unique_ptr<CBVFrameVariable> m_pCBVFrameVariable = nullptr;
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