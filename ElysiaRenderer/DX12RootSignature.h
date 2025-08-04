#pragma once
#include "stdafx.h"

namespace ElysiaRenderer
{


	class DX12RootParameter
	{
		friend class DX12RootSignature;

	public:
		DX12RootParameter();
		~DX12RootParameter();
		

		void InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE rangeType, UINT numDescriptors, UINT slotIndex, D3D12_SHADER_VISIBILITY shaderVisibility = D3D12_SHADER_VISIBILITY_ALL);
		void InitAsDescriptorTable(UINT rangeCount, D3D12_SHADER_VISIBILITY shaderVisibility = D3D12_SHADER_VISIBILITY_ALL);
		void SetTableRange(D3D12_DESCRIPTOR_RANGE_TYPE rangeType, UINT numDescriptors, UINT slotIndex, UINT rangeIndex, UINT space = 0);

		void Clear();

	private:
		D3D12_ROOT_PARAMETER m_rootParamter;
	};

	struct RootSignatureCreatDesc
	{
		std::vector<DX12RootParameter*> rootParamters;
	};

	class DX12RootSignature
	{
	public:
		DX12RootSignature(UINT numRootParams = 0, UINT numStaticSamplers = 0);
		DX12RootSignature(ID3D12RootSignature* rootSignature, UINT numRootParams = 0, UINT numStaticSamplers = 0);

		~DX12RootSignature();

		ID3D12RootSignature* GetSignature() const
		{
			return m_rootSignature;
		}

		void InitStaticSamplers(UINT slotIndex, const D3D12_SAMPLER_DESC& nonStaticSamplerDesc, D3D12_SHADER_VISIBILITY shaderVisibility = D3D12_SHADER_VISIBILITY_ALL);
		void Init(ID3D12Device5* device, D3D12_ROOT_SIGNATURE_FLAGS Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE);

		void Reset(UINT numRootParams = 0, UINT numStaticSamplers = 0);

		DX12RootParameter& operator[](size_t index)
		{
			assert(index < m_numRootParameters);
			return m_rootParametersArray[index];
		}
		const DX12RootParameter& operator[](size_t index) const
		{
			assert(index < m_numRootParameters);
			return m_rootParametersArray[index];
		}

	private:
		bool m_isInited;
		UINT m_numRootParameters;
		UINT m_numSamplers;
		UINT m_numInitedSamplers;
		ID3D12RootSignature* m_rootSignature;
		std::unique_ptr<DX12RootParameter[]> m_rootParametersArray;
		std::unique_ptr<D3D12_STATIC_SAMPLER_DESC[]> m_samplerArray;
		UINT32 m_descriptorTableSize[16];	// Non-sampler descriptor tables need to know their descriptor count. one index for one root parameter
	};
}