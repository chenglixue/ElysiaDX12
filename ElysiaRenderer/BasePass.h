#pragma once
#include "stdafx.h"
#include "RenderPassData.h"

namespace ElysiaRenderer
{
	class BasePass
	{
	public:
		BasePass() : 
			m_renderSize(0, 0)
		{

		}
		virtual ~BasePass()
		{
			Dispose();
		}

		virtual void Setup(const RenderPassData& renderPassData)
		{
			m_renderSize = renderPassData.RenderSize;
			m_pCommand = renderPassData.pCommand;
			m_pGraphicsPipelineStates = renderPassData.pGraphicsPipelineStates;

			Configure();
		}
		virtual void Configure() = 0;
		virtual void Execute() = 0;
		virtual void Render()
		{
			Execute();
		}

		virtual void Dispose() = 0;

	protected:
		UINT2 m_renderSize;
		DX12GraphicsContext* m_pCommand = nullptr;
		std::unordered_map<UINT, std::shared_ptr<PipelineStateObject>>* m_pGraphicsPipelineStates = nullptr;
	};
}