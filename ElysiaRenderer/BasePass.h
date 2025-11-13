#pragma once
#include "ModelImporter.h"
#include "RenderMaterial.h"
#include "RenderPassData.h"
#include "UserData.h"
#include "PSOManager.h"
#include "DX12Camera.h"
#include "PIXHelper.h"

namespace ElysiaRenderer
{
	using namespace ElysiaModel;

	class RenderTexture;
	class DX12GraphicsContext;

	class BasePass
	{
	public:
		BasePass() = default;
		BasePass(DX12Camera* pCamera);
		virtual ~BasePass();

		virtual void Setup(const RenderPassData& renderPassData);
		virtual void Configure() = 0;
		virtual void Execute() = 0;
		virtual void Render() = 0;

		virtual void Dispose();

	protected:
		Vector2 m_renderSize;
		DX12GraphicsContext* m_pCommand = nullptr;
		DX12Camera* m_pCamera = nullptr;

		std::vector<ShaderPass> m_shaderPasses;
		std::unique_ptr<ElysiaRenderer::RenderMaterial> m_pMaterial = nullptr;
		std::unordered_map<UINT, PipelineStateObject*> m_PipelineStateObjects;
	};
}