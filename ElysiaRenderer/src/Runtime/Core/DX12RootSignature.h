#pragma once

namespace ElysiaCore
{
	class DX12RootParameter
	{
		friend class DX12RootSignature;

	public:
		DX12RootParameter();
		~DX12RootParameter();
		
		D3D12_ROOT_PARAMETER1& GetRootParameter()
		{
			return m_rootParamter;
		}
		D3D12_ROOT_PARAMETER_TYPE GetType()
		{
			return m_rootParamter.ParameterType;
		}
		UINT GetSpaceID()
		{
			return m_spaceID;
		}


		void InitAsConstantBufferView(UINT slotIndex, D3D12_SHADER_VISIBILITY shaderVisibility = D3D12_SHADER_VISIBILITY_ALL, UINT Space = 0);
		/*void InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE rangeType, UINT numDescriptors, UINT slotIndex, 
			const D3D12_DESCRIPTOR_RANGE1* descriptorRangeData, D3D12_DESCRIPTOR_RANGE_FLAGS flags, D3D12_SHADER_VISIBILITY shaderVisibility = D3D12_SHADER_VISIBILITY_ALL);*/
		void InitAsDescriptorTable(UINT rangeCount, const D3D12_DESCRIPTOR_RANGE1* descriptorRangeData, D3D12_SHADER_VISIBILITY shaderVisibility = D3D12_SHADER_VISIBILITY_ALL);
		void SetTableRange(D3D12_DESCRIPTOR_RANGE_TYPE rangeType, UINT numDescriptors, UINT slotIndex, UINT rangeIndex, D3D12_DESCRIPTOR_RANGE_FLAGS flags, UINT space = 0);

		void Clear();

	private:
		D3D12_ROOT_PARAMETER1 m_rootParamter;
		UINT m_spaceID;
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

		CComPtr<ID3D12RootSignature> GetSignature() const
		{
			return m_rootSignature;
		}
		UINT GetNumRootParams()
		{
			return m_numRootParameters;
		}
		DX12RootParameter* GetDX12RootParameters()
		{
			return m_rootParametersArray.get();
		}

		void InitStaticSamplers(UINT slotIndex, const D3D12_SAMPLER_DESC& nonStaticSamplerDesc, D3D12_SHADER_VISIBILITY shaderVisibility = D3D12_SHADER_VISIBILITY_ALL);
		void Init(ID3D12Device* device, D3D12_ROOT_SIGNATURE_FLAGS Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE);

		void Reset(UINT numRootParams = 0, UINT numStaticSamplers = 0);

		DX12RootParameter& GetRootParameter(size_t index)
		{
			return m_rootParametersArray[index];
		}
		D3D12_ROOT_PARAMETER1* GetRootParameters()
		{
			D3D12_ROOT_PARAMETER1* rootParameters = new D3D12_ROOT_PARAMETER1[m_numRootParameters];
			for (UINT i = 0; i < m_numRootParameters; ++i)
			{
				rootParameters[i] = m_rootParametersArray[i].GetRootParameter();
			}

			return rootParameters;
		}
		
		void AddRootParameter(DX12RootParameter& rootParameter, size_t index)
		{
			assert(index < m_numRootParameters);
			m_rootParametersArray[index] = rootParameter;
		}

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
		CComPtr<ID3D12RootSignature> m_rootSignature;
		std::unique_ptr<DX12RootParameter[]> m_rootParametersArray;
		std::unique_ptr<D3D12_STATIC_SAMPLER_DESC[]> m_samplerArray;
		UINT32 m_descriptorTableSize[16];	// Non-sampler descriptor tables need to know their descriptor count. one index for one root parameter
	};
}