#pragma once
#include "Helper.h"
#include "PipelineResourceSpace.h"
#include "PipelineResourceUtility.h"
#include "../DX12/DX12RootSignature.h"
#include "Hash.h"
#include <iostream>
#include <functional>

namespace ElysiaRenderer
{
	class DX12Shader;
	class DX12RootSignature;
	class DX12TextureResource;
	class Material;

	enum class PipelineType : uint8_t
	{
		None = 0,
		Graphics = 1,
		Compute = 2
	};

	struct RenderTargetDesc
	{
		std::array<DXGI_FORMAT, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT> m_renderTargetFormats{ DXGI_FORMAT_UNKNOWN };
		uint8_t m_numRenderTargets = 0;
		DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_UNKNOWN;
	};

	struct PSODescriptor
	{
		Material* pRenderMaterial;
		UINT ShaderPassIndex;
		RenderTargetDesc RTDesc;
	};

	inline RenderTargetDesc CreateDefaultRenderTargetDesc()
	{
		RenderTargetDesc desc{};
		desc.m_renderTargetFormats.fill(DXGI_FORMAT_UNKNOWN);
		desc.m_numRenderTargets = 1;
		desc.m_depthStencilFormat = DXGI_FORMAT_UNKNOWN;
		return desc;
	}
}

namespace std
{
	using namespace ElysiaRenderer;

	template<>
	struct hash<RenderTargetDesc>
	{
		size_t operator()(RenderTargetDesc const& key) const
		{
			size_t value = key.m_depthStencilFormat;
			value += key.m_numRenderTargets;
			for (UINT i = 0; i < key.m_numRenderTargets; ++i)
			{
				value += key.m_renderTargetFormats[i];
			}

			hash<size_t> h;
			return h(value);
		}
	};

	template<>
	struct hash<PSODescriptor>
	{
		size_t operator()(const PSODescriptor& key) const
		{
			size_t value = key.ShaderPassIndex;
			value += reinterpret_cast<size_t>(key.pRenderMaterial);
			value += hash<RenderTargetDesc>()(key.RTDesc);
			
			hash<size_t> h;
			return h(value);
		}
	};

	template<>
	struct equal_to<ElysiaRenderer::PSODescriptor>
	{
		using argument_type = ElysiaRenderer::PSODescriptor;
		using result_type = size_t;

		bool operator()(argument_type const& a, argument_type const& b) const
		{
			return memcmp(&a, &b, sizeof(argument_type)) == 0;
		}
	};
}