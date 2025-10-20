#include "GBufferPass.h"

namespace ElysiaRenderer
{
	GBufferPass::~GBufferPass()
	{
		Dispose();
	}

	void GBufferPass::Configure()
	{
		RenderTextureDesc RTCreateDesc{};

		// Base Color , ShadingModel
		{
			auto pGBufferRT = std::make_unique<RenderTexture>();

			RTCreateDesc.Name = L"GBuffer_0";
			RTCreateDesc.Width = static_cast<UINT64>(m_renderSize.x);
			RTCreateDesc.Height = static_cast<UINT>(m_renderSize.y);
			RTCreateDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			RTCreateDesc.Dimension = TextureDimension::Tex2D;
			RTCreateDesc.ArraySize = 1;
			RTCreateDesc.EnableRandomWrite = false;
			RTCreateDesc.MipmapLevels = 1;
			RTCreateDesc.MSAASamples = 1;

			pGBufferRT->Init(RTCreateDesc);
			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
		}
			
		// Metallic, Specular, Roughness, AO
		{
			auto pGBufferRT = std::make_unique<RenderTexture>();

			RTCreateDesc.Name = L"GBuffer_1";
			RTCreateDesc.Width = static_cast<UINT64>(m_renderSize.x);
			RTCreateDesc.Height = static_cast<UINT>(m_renderSize.y);
			RTCreateDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			RTCreateDesc.Dimension = TextureDimension::Tex2D;
			RTCreateDesc.ArraySize = 1;
			RTCreateDesc.EnableRandomWrite = false;
			RTCreateDesc.MipmapLevels = 1;
			RTCreateDesc.MSAASamples = 1;

			pGBufferRT->Init(RTCreateDesc);
			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
		}

		// Encode World Tangent, Anisotropy
		{
			auto pGBufferRT = std::make_unique<RenderTexture>();

			RTCreateDesc.Name = L"GBuffer_2";
			RTCreateDesc.Width = static_cast<UINT64>(m_renderSize.x);
			RTCreateDesc.Height = static_cast<UINT>(m_renderSize.y);
			RTCreateDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			RTCreateDesc.Dimension = TextureDimension::Tex2D;
			RTCreateDesc.ArraySize = 1;
			RTCreateDesc.EnableRandomWrite = false;
			RTCreateDesc.MipmapLevels = 1;
			RTCreateDesc.MSAASamples = 1;

			pGBufferRT->Init(RTCreateDesc);
			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
		}

		// Encode World Normal, per object data
		{
			auto pGBufferRT = std::make_unique<RenderTexture>();

			RTCreateDesc.Name = L"GBuffer_3";
			RTCreateDesc.Width = static_cast<UINT64>(m_renderSize.x);
			RTCreateDesc.Height = static_cast<UINT>(m_renderSize.y);
			RTCreateDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
			RTCreateDesc.Dimension = TextureDimension::Tex2D;
			RTCreateDesc.ArraySize = 1;
			RTCreateDesc.EnableRandomWrite = false;
			RTCreateDesc.MipmapLevels = 1;
			RTCreateDesc.MSAASamples = 1;

			pGBufferRT->Init(RTCreateDesc);
			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
		}

		// Emission, opacity
		{
			auto pGBufferRT = std::make_unique<RenderTexture>();

			RTCreateDesc.Name = L"GBuffer_4";
			RTCreateDesc.Width = static_cast<UINT64>(m_renderSize.x);
			RTCreateDesc.Height = static_cast<UINT>(m_renderSize.y);
			RTCreateDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
			RTCreateDesc.Dimension = TextureDimension::Tex2D;
			RTCreateDesc.ArraySize = 1;
			RTCreateDesc.EnableRandomWrite = false;
			RTCreateDesc.MipmapLevels = 1;
			RTCreateDesc.MSAASamples = 1;

			pGBufferRT->Init(RTCreateDesc);
			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
		}

		// Velocity
		{
			auto pGBufferRT = std::make_unique<RenderTexture>();

			RTCreateDesc.Name = L"GBuffer_4";
			RTCreateDesc.Width = static_cast<UINT64>(m_renderSize.x);
			RTCreateDesc.Height = static_cast<UINT>(m_renderSize.y);
			RTCreateDesc.Format = DXGI_FORMAT_R16G16B16A16_SNORM;
			RTCreateDesc.Dimension = TextureDimension::Tex2D;
			RTCreateDesc.ArraySize = 1;
			RTCreateDesc.EnableRandomWrite = false;
			RTCreateDesc.MipmapLevels = 1;
			RTCreateDesc.MSAASamples = 1;

			pGBufferRT->Init(RTCreateDesc);
			m_GBufferRTs.emplace_back(std::move(pGBufferRT));
		}
	}

	void GBufferPass::Execute()
	{

	}

	void GBufferPass::Render()
	{
		Execute();
	}

	void GBufferPass::Dispose()
	{
		m_GBufferRTs.clear();
	}
}