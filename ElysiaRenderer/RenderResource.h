#pragma once
#include "stdafx.h"
#include "DX12Device.h"
#include "CBVParameter.h"

namespace ElysiaRenderer
{

	class RenderResource
	{
	public:
		RenderResource() = default;
		RenderResource(DX12Device* device);
		RenderResource(const RenderResource& rhs) = delete;
		RenderResource& operator=(RenderResource& rhs) = delete;
		RenderResource(RenderResource&& rhs) = default;
		~RenderResource();

		CBVMainPassParameter* GetCBVPassParameter();

	private:
		DX12Device* m_device = nullptr;
		std::unique_ptr<CBVMainPassParameter> m_pCBVPassParameter = nullptr;
	};

}