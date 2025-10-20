#pragma once
#include "stdafx.h"
#include "RenderPassData.h"

namespace ElysiaRenderer
{
	class BasePass
	{
	public:
		virtual ~BasePass()
		{
			Dispose();
		}

		virtual void Setup(const RenderPassData& renderPassData)
		{
			m_renderSize = renderPassData.RenderSize;
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
	};
}