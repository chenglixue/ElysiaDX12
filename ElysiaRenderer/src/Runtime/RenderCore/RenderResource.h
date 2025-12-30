#pragma once
#include "Runtime/RenderCore/CBVParameter.h"
#include "ThirdParty/FreesyncHDR.h"

namespace ElysiaCore
{
	class PipelineResourceSpace;
	class DX12Device;
}

namespace ElysiaRenderer
{
	using namespace ElysiaCore;
	
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

		PipelineResourceSpace* GetPerFrameBindResourceSpace(UINT frameID);
		CBVFrameVariable& GetCBVFrameVariable();
		CAULDRON_DX12::DisplayMode GetDisplayMode() const noexcept;
		std::string GetShaderConstantVariable(size_t hash) const noexcept;
		
		void SetDisplayMode(CAULDRON_DX12::DisplayMode newDisplayMode);
		
		size_t AddPropertyID(const std::wstring& name);
		std::wstring GetPropertyName(size_t hash) const noexcept;

		std::unordered_map<size_t, std::wstring> m_nameHashs;
		std::array<std::unique_ptr<PipelineResourceSpace>, ElysiaHelper::NUM_FRAMES_IN_FLIGHT> m_perFrameBindResourceSpaces;
	private:
		DX12Device* m_device = nullptr;
		static std::unique_ptr<RenderResource> m_instance;
		static std::once_flag m_initInstanceFlag;
		
		CBVFrameVariable m_CBVFrameVariable;

		CAULDRON_DX12::DisplayMode m_currDisplayMode = CAULDRON_DX12::DisplayMode::DISPLAYMODE_SDR;
	};

	size_t PropertyToID(const std::wstring& name);
}