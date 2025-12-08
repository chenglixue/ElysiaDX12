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

		static RenderResource& GetInstance()
		{
			std::call_once(m_initInstanceFlag, []() {
				m_instance.reset(new RenderResource());
				});

			return *m_instance;
		}

		PipelineResourceSpace* GetPerFrameBindResourceSpace();
		CBVFrameVariable* GetCBVFrameVariable();
		CAULDRON_DX12::DisplayMode GetDisplayMode() const noexcept;
		std::string GetShaderConstantVariable(size_t hash) const noexcept;
		
		void SetDisplayMode(CAULDRON_DX12::DisplayMode newDisplayMode);
		
		size_t AddPropertyID(const std::string& name);
		std::string GetPropertyName(size_t hash) const noexcept;

		std::unordered_map<size_t, std::string> m_nameHashs;
	private:
		DX12Device* m_device = nullptr;
		static std::unique_ptr<RenderResource> m_instance;
		static std::once_flag m_initInstanceFlag;
		
		std::unique_ptr<PipelineResourceSpace> m_perFrameBindResourceSpace;
		std::unique_ptr<CBVFrameVariable> m_pCBVFrameVariable = nullptr;

		CAULDRON_DX12::DisplayMode m_currDisplayMode = CAULDRON_DX12::DisplayMode::DISPLAYMODE_SDR;
	};

	size_t PropertyToID(const std::string& name);
}