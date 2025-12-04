#pragma once
#include "lib/Utility/Helper.h"
#include "IManager.h"
#include "lib/Utility/Hash.h"
#include <iostream>
#include <functional>
#include "lib/Utility/PipelineStateUtility.h"

namespace std
{
	template<>
	struct hash<D3D12_GRAPHICS_PIPELINE_STATE_DESC>
	{
		using argument_type = D3D12_GRAPHICS_PIPELINE_STATE_DESC;
		using result_type = size_t;

		size_t operator()(argument_type const& v) const
		{
			return xxh::GetHash<argument_type>(v);
		}
	};

	template<>
	struct equal_to<D3D12_GRAPHICS_PIPELINE_STATE_DESC> 
	{
		using argument_type = D3D12_GRAPHICS_PIPELINE_STATE_DESC;
		using result_type = size_t;

		bool operator()(argument_type const& a, argument_type const& b) const 
		{
			return memcmp(&a, &b, sizeof(argument_type)) == 0;
		}
	};

	template<>
	struct hash<D3D12_COMPUTE_PIPELINE_STATE_DESC>
	{
		using argument_type = D3D12_COMPUTE_PIPELINE_STATE_DESC;
		using result_type = size_t;

		size_t operator()(argument_type const& v) const
		{
			return xxh::GetHash<argument_type>(v);
		}
	};

	template<>
	struct equal_to<D3D12_COMPUTE_PIPELINE_STATE_DESC>
	{
		using argument_type = D3D12_COMPUTE_PIPELINE_STATE_DESC;
		using result_type = size_t;

		bool operator()(argument_type const& a, argument_type const& b) const
		{
			return memcmp(&a, &b, sizeof(argument_type)) == 0;
		}
	};
}

namespace ElysiaRenderer
{

	class Material;
	struct PipelineStateObject;
	class DX12Device;

	class PSOManager : IManager
	{
	public:
		PSOManager() = default;
		PSOManager(const PSOManager& rhs) = delete;
		PSOManager& operator=(PSOManager& rhs) = delete;
		PSOManager(PSOManager&& rhs) = default;
		~PSOManager();

		virtual void Init() override;
		virtual void Destory() override;

		PipelineStateObject* GetGraphicsPipelineState(DX12Device* pDevice, Material* pMaterial, UINT passIndex,
			const RenderTargetDesc& renderTargetDesc,
			D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

		PipelineStateObject* GetComputePipelineState(DX12Device* pDevice, Material* pMaterial, UINT passIndex);

	private:
		std::unordered_map<D3D12_GRAPHICS_PIPELINE_STATE_DESC, std::unique_ptr<PipelineStateObject>> m_graphicsPipelineStates{};
		std::unordered_map<D3D12_COMPUTE_PIPELINE_STATE_DESC, std::unique_ptr<PipelineStateObject>> m_computePipelineStates{};

		PipelineStateObject* GetGraphicsPipelineState(DX12Device* pDevice, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& PSODesc, DX12RootSignature* pRootSignature);
		PipelineStateObject* GetComputePipelineState(DX12Device* pDevice, const D3D12_COMPUTE_PIPELINE_STATE_DESC& PSODesc, DX12RootSignature* pRootSignature);
	};

	extern std::unique_ptr<PSOManager> g_pPSOManager;
	inline static PSOManager* GetPSOManager()
	{
		if (g_pPSOManager == nullptr)
		{
			ThrowRuntimeError("null PSO manager");
		}
		return g_pPSOManager.get();
	}
}