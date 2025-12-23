#pragma once
#include "Runtime/RenderCore/Material.h"
#include "RenderPassData.h"
#include "Editor/UserData.h"
#include "../PSOManager.h"
#include "../DX12Camera.h"
#include "Programs/PIXHelper.h"
#include "../ShaderVariantManager.h"


namespace ElysiaRenderer
{
	class RenderTexture;
	class ElysiaCore::DX12GraphicsContext;
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
		
		D3D12_GPU_VIRTUAL_ADDRESS  UploadMaterialConstants(
			UploadRingBuffer* pUploadBuffer,
			UINT8 spaceID,
			Material* pMaterial,
			const ShaderVariantData* pVariantData,
			size_t passID = 0);

	protected:
		Vector2 m_renderSize;
		DX12Device* m_pDevice = nullptr;
		DX12GraphicsContext* m_pCommand = nullptr;
		DX12Camera* m_pCamera = nullptr;
		RenderTexture* m_pCameraColorRT = nullptr;
		RenderTexture* m_pCameraDepthRT = nullptr;

		std::vector<ShaderPass> m_shaderPasses;
		std::unique_ptr<Material> m_pMaterial = nullptr;
		std::unordered_map<UINT, PipelineStateObject*> m_PipelineStateObjects;

		void SetSpaceResource(PassData& passData, UINT8 spaceID);
	};
}