#pragma once
#include "stdafx.h"
#include "lib/DX12/DX12Device.h"
#include "Parameter/CBVParameter.h"
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

		PipelineResourceSpace* GetPerObjectBindResourceSpace();
		PipelineResourceSpace* GetPerFrameBindResourceSpace();
		CBVFrameVariable* GetCBVFrameVariable();
		CAULDRON_DX12::DisplayMode GetDisplayMode() const noexcept;
		std::string GetShaderConstantVariable(size_t hash) const noexcept;
		
		void SetDisplayMode(CAULDRON_DX12::DisplayMode newDisplayMode);


	private:

		std::unique_ptr<PipelineResourceSpace> m_perObjectBindResourceSpace;
		std::unique_ptr<PipelineResourceSpace> m_perFrameBindResourceSpace;

		DX12Device* m_device = nullptr;
		std::unique_ptr<CBVFrameVariable> m_pCBVFrameVariable = nullptr;

		CAULDRON_DX12::DisplayMode m_currDisplayMode = CAULDRON_DX12::DisplayMode::DISPLAYMODE_SDR;
	};

	extern std::unordered_map<size_t, std::string> g_shaderConstantVariables;
	extern std::unique_ptr<RenderResource> g_pRenderResource;
	inline RenderResource* GetRenderResource()
	{
		if (g_pRenderResource == nullptr)
		{
			ThrowRuntimeError("Null Render Resource");
		}
		return g_pRenderResource.get();
	}

	void AddShaderConstantVariable(const size_t hash, const std::string& name);
	size_t PropertyToID(const std::string& name);

}