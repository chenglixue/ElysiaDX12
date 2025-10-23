#pragma once
#include "stdafx.h"
#include "Common.h"
#include "RenderPassData.h"
#include "TextureManager.h"
#include "ModelImporter.h"
#include "RenderResource.h"
#include "UserData.h"

namespace ElysiaRenderer
{
	using namespace ElysiaModel;

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

		virtual void Dispose()
		{
		};

	protected:
		UINT2 m_renderSize;
		DX12GraphicsContext* m_pCommand = nullptr;
		std::unordered_map<UINT, std::unique_ptr<PipelineStateObject>>* m_pGraphicsPipelineStates = nullptr;
	};
}