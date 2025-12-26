#pragma once
#include "Runtime/Core/ShaderUtility.h"
#include "Runtime/Engine/FrameContext.h"

namespace ElysiaCore
{
	class DX12GraphicsContext;
	class DX12Device;
	class UploadRingBuffer;
	struct PipelineStateObject;
	class SwapChain;
}

namespace ElysiaRenderer
{
	class RenderTexture;
	class DX12Camera;
	class Material;
	struct PassData;
	struct RenderPassData;
}

namespace ElysiaRenderer
{
	using namespace ElysiaCore;
	using namespace ElysiaRenderer;
	
	class BasePass
	{
	public:
		BasePass() = default;
		BasePass(DX12Camera* pCamera);
		virtual ~BasePass();

		virtual void Setup(const RenderPassData& renderPassData);
		virtual void Configure() = 0;
		virtual void Render(ElysiaEngine::FrameContext& context) = 0;

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
		SwapChain* m_pSwaiChain = nullptr;
		DX12Camera* m_pCamera = nullptr;
		RenderTexture* m_pCameraColorRT = nullptr;
		RenderTexture* m_pCameraDepthRT = nullptr;

		std::vector<ShaderPass> m_shaderPasses;
		std::unique_ptr<Material> m_pMaterial = nullptr;
		std::unordered_map<UINT, PipelineStateObject*> m_PipelineStateObjects;

		void SetSpaceResource(PassData& passData, UINT8 spaceID);
	};
}