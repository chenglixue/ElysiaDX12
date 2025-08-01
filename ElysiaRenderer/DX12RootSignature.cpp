#include"DX12RootSignature.h"

namespace ElysiaRenderer
{
	DX12RootParameter::DX12RootParameter()
	{
		m_rootParamter.ParameterType = (D3D12_ROOT_PARAMETER_TYPE)0xFFFFFFFF;
	}
	DX12RootParameter::~DX12RootParameter()
	{
		Clear();
	}

	void DX12RootParameter::Clear()
	{
		m_rootParamter.ParameterType = (D3D12_ROOT_PARAMETER_TYPE)0xFFFFFFFF;

		if (m_rootParamter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			delete[]m_rootParamter.DescriptorTable.pDescriptorRanges;
		}
	}

	/// <summary>
	/// init as DescriptorTable, only has one range
	/// </summary>
	/// <param name="rangeType"></param>
	/// <param name="numDescriptors"></param>
	/// <param name="slotIndex"></param>
	/// <param name="shaderVisibility"></param>
	void DX12RootParameter::InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE rangeType, UINT numDescriptors,
		UINT slotIndex, D3D12_SHADER_VISIBILITY shaderVisibility)
	{
		InitAsDescriptorTable(1, shaderVisibility);
		SetTableRange(rangeType, numDescriptors, slotIndex, 0);
	}

	/// <summary>
	/// can init mult ranges
	/// </summary>
	/// <param name="rangeCount"></param>
	/// <param name="shaderVisibility"></param>
	void DX12RootParameter::InitAsDescriptorTable(UINT rangeCount, D3D12_SHADER_VISIBILITY shaderVisibility)
	{
		m_rootParamter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		m_rootParamter.ShaderVisibility = shaderVisibility;
		m_rootParamter.DescriptorTable.NumDescriptorRanges = rangeCount;
		m_rootParamter.DescriptorTable.pDescriptorRanges = new D3D12_DESCRIPTOR_RANGE[rangeCount];
	}

	/// <summary>
	/// after init descriptor table, can set mult range
	/// </summary>
	/// <param name="rangeType"></param>
	/// <param name="numDescriptors"></param>
	/// <param name="slotIndex"></param>
	/// <param name="rangeIndex"></param>
	/// <param name="space"></param>
	/// <returns></returns>
	void DX12RootParameter::SetTableRange(D3D12_DESCRIPTOR_RANGE_TYPE rangeType, UINT numDescriptors,
		UINT slotIndex, UINT rangeIndex, UINT space)
	{
		D3D12_DESCRIPTOR_RANGE* range = const_cast<D3D12_DESCRIPTOR_RANGE*>(m_rootParamter.DescriptorTable.pDescriptorRanges + rangeIndex);
		range->RangeType = rangeType;
		range->NumDescriptors = numDescriptors;
		range->BaseShaderRegister = slotIndex;
		range->RegisterSpace = space;
		range->OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	}





	DX12RootSignature::DX12RootSignature(UINT numRootParams, UINT numStaticSamplers) : 
		m_isInited(false), m_numRootParameters(numRootParams)
	{
		Reset(numRootParams, numStaticSamplers);
	}

	DX12RootSignature::DX12RootSignature(ID3D12RootSignature* rootSignature, UINT numRootParams, UINT numStaticSamplers) :
		m_rootSignature(rootSignature), m_numRootParameters(numRootParams)
	{
		Reset(numRootParams, numStaticSamplers);
	}

	DX12RootSignature::~DX12RootSignature()
	{
		ElysiaHelper::SafeRelease(m_rootSignature);
	}

	void DX12RootSignature::InitStaticSamplers(UINT slotIndex, const D3D12_SAMPLER_DESC& nonStaticSamplerDesc, D3D12_SHADER_VISIBILITY shaderVisibility)
	{
		assert(m_numInitedSamplers < m_numSamplers);
		D3D12_STATIC_SAMPLER_DESC& staticSamplerDesc = m_samplerArray[m_numInitedSamplers++];

		staticSamplerDesc.Filter = nonStaticSamplerDesc.Filter;
		staticSamplerDesc.AddressU = nonStaticSamplerDesc.AddressU;
		staticSamplerDesc.AddressV = nonStaticSamplerDesc.AddressV;
		staticSamplerDesc.AddressW = nonStaticSamplerDesc.AddressW;
		staticSamplerDesc.MipLODBias = nonStaticSamplerDesc.MipLODBias;
		staticSamplerDesc.MaxAnisotropy = nonStaticSamplerDesc.MaxAnisotropy;
		staticSamplerDesc.ComparisonFunc = nonStaticSamplerDesc.ComparisonFunc;
		staticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
		staticSamplerDesc.MinLOD = nonStaticSamplerDesc.MinLOD;
		staticSamplerDesc.MaxLOD = nonStaticSamplerDesc.MaxLOD;
		staticSamplerDesc.ShaderRegister = slotIndex;
		staticSamplerDesc.RegisterSpace = 0;
		staticSamplerDesc.ShaderVisibility = shaderVisibility;
	}

	void DX12RootSignature::Init(ID3D12Device5* device, D3D12_ROOT_SIGNATURE_FLAGS flags)
	{
		if (m_isInited) return;
		assert(m_numInitedSamplers == m_numSamplers);

		/*D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.Desc_1_0.NumParameters = m_numRootParameters;
		rootSignatureDesc.Desc_1_0.pParameters = (const D3D12_ROOT_PARAMETER*)m_rootParametersArray.get();
		rootSignatureDesc.Desc_1_0.NumStaticSamplers = m_numSamplers;
		rootSignatureDesc.Desc_1_0.pStaticSamplers = (const D3D12_STATIC_SAMPLER_DESC*)m_samplerArray.get();
		rootSignatureDesc.Desc_1_0.Flags = flags;*/
		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.NumParameters = m_numRootParameters;
		rootSignatureDesc.pParameters = (const D3D12_ROOT_PARAMETER*)m_rootParametersArray.get();
		rootSignatureDesc.NumStaticSamplers = m_numSamplers;
		rootSignatureDesc.pStaticSamplers = (const D3D12_STATIC_SAMPLER_DESC*)m_samplerArray.get();
		rootSignatureDesc.Flags = flags;

		for (int i = 0; i < m_numRootParameters; ++i)
		{
			const D3D12_ROOT_PARAMETER& rootParam = rootSignatureDesc.pParameters[i];
			m_descriptorTableSize[i] = 0;

			if (rootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
			{
				assert(rootParam.DescriptorTable.pDescriptorRanges != nullptr);

				for (UINT rangeIndex = 0; rangeIndex < rootParam.DescriptorTable.NumDescriptorRanges; ++rangeIndex)
				{
					m_descriptorTableSize[i] += rootParam.DescriptorTable.pDescriptorRanges[rangeIndex].NumDescriptors;
				}
			}
		}

		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1;

		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		ID3DBlob* signature = nullptr;
		ID3DBlob* error = nullptr;
		//ElysiaHelper::ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error));
		ElysiaHelper::ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));

		//ID3D12RootSignature* rootSignature = nullptr;
		ElysiaHelper::ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
	
		m_isInited = true;
	}

	void DX12RootSignature::Reset(UINT numRootParams, UINT numStaticSamplers)
	{
		if (numRootParams > 0)
		{
			m_rootParametersArray.reset(new DX12RootParameter[numRootParams]);
		}
		else
		{
			m_rootParametersArray = nullptr;
		}

		if (numStaticSamplers > 0)
		{
			m_samplerArray.reset(new D3D12_STATIC_SAMPLER_DESC[numStaticSamplers]);
		}
		else
		{
			m_samplerArray = nullptr;
		}

		m_numSamplers = numStaticSamplers;
		m_numInitedSamplers = 0;
	}
}