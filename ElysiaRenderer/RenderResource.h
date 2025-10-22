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

		static RenderResource& GetInstance()
		{
			std::call_once(m_initInstanceFlag, []() {
				m_instance.reset(new RenderResource());
				});

			return *m_instance;
		}

		static PipelineResourceSpace* GetPerObjectBindResourceSpace()
		{
			return m_perObjectBindResourceSpace.get();
		}
		static PipelineResourceSpace* GetPerMainBindResourceSpace()
		{
			return m_perMainPassBindResourceSpace.get();
		}

		CBVMainPassParameter* GetCBVPassParameter();

	private:
		static std::unique_ptr<RenderResource> m_instance;
		static std::once_flag m_initInstanceFlag;

		static std::unique_ptr<PipelineResourceSpace> m_perObjectBindResourceSpace;
		static std::unique_ptr<PipelineResourceSpace> m_perMainPassBindResourceSpace;

		DX12Device* m_device = nullptr;
		std::unique_ptr<CBVMainPassParameter> m_pCBVPassParameter = nullptr;
	};

}