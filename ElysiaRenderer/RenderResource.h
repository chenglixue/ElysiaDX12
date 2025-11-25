#pragma once
#include "stdafx.h"
#include "DX12Device.h"
#include "CBVParameter.h"
#include "AMD/LPM/FreesyncHDR.h"

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
		void SetDisplayMode(CAULDRON_DX12::DisplayMode newDisplayMode)
		{
			m_currDisplayMode = newDisplayMode;
		}
		CAULDRON_DX12::DisplayMode GetDisplayMode() const noexcept
		{
			return m_currDisplayMode;
		}

	private:

		std::unique_ptr<PipelineResourceSpace> m_perObjectBindResourceSpace;
		std::unique_ptr<PipelineResourceSpace> m_perFrameBindResourceSpace;

		DX12Device* m_device = nullptr;
		std::unique_ptr<CBVFrameVariable> m_pCBVFrameVariable = nullptr;

		CAULDRON_DX12::DisplayMode m_currDisplayMode = CAULDRON_DX12::DisplayMode::DISPLAYMODE_SDR;
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