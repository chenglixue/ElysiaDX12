#pragma once
#include "lib/Model//ModelImporter.h"
#include "src/Material.h"
#include "RenderPassData.h"
#include "Parameter/UserData.h"
#include "Manager/PSOManager.h"
#include "lib/DX12/DX12Camera.h"
#include "lib/Utility/PIXHelper.h"
#include "Manager/ShaderVariantManager.h"


namespace ElysiaRenderer
{
	using namespace ElysiaModel;

	class RenderTexture;
	class DX12GraphicsContext;
	class UploadRingBuffer;

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

		virtual void UpdatePSO() = 0;
		virtual void UpdateVariant() = 0;
		
		

	protected:
		Vector2 m_renderSize;
		DX12Device* m_pDevice = nullptr;
		DX12GraphicsContext* m_pCommand = nullptr;
		DX12Camera* m_pCamera = nullptr;

		std::vector<ShaderPass> m_shaderPasses;
		std::unique_ptr<Material> m_pMaterial = nullptr;
		std::unordered_map<UINT, PipelineStateObject*> m_PipelineStateObjects;

		void SetSpaceResource(PassData& passData, UINT8 spaceID);
	};
}