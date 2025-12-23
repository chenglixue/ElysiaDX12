#include "stdafx.h"
#include "Programs/Helper.h"
#include"DX12RootSignature.h"

namespace ElysiaCore
{
	using namespace ElysiaHelper;

	DX12RootParameter::DX12RootParameter()
	{
		m_rootParamter.ParameterType = (D3D12_ROOT_PARAMETER_TYPE)0xFFFFFFFF;
		m_rootParamter.Descriptor = {};
		m_rootParamter.DescriptorTable = {};
		m_rootParamter.Descriptor = {};
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

	void DX12RootParameter::InitAsConstantBufferView(UINT slotIndex, D3D12_SHADER_VISIBILITY shaderVisibility, UINT Space)
	{
		m_rootParamter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		m_rootParamter.ShaderVisibility = shaderVisibility;
		m_rootParamter.Descriptor.ShaderRegister = slotIndex;
		m_rootParamter.Descriptor.RegisterSpace = m_spaceID = Space;
	}

	/// <summary>
	/// init as DescriptorTable, only has one range
	/// </summary>
	/// <param name="rangeType"></param>
	/// <param name="numDescriptors"></param>
	/// <param name="slotIndex"></param>
	/// <param name="shaderVisibility"></param>
	/*void DX12RootParameter::InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE rangeType, UINT numDescriptors,
		UINT slotIndex, const D3D12_DESCRIPTOR_RANGE1* descriptorRangeData, D3D12_DESCRIPTOR_RANGE_FLAGS flags, D3D12_SHADER_VISIBILITY shaderVisibility)
	{
		InitAsDescriptorTable(1, descriptorRangeData, shaderVisibility);
		SetTableRange(rangeType, numDescriptors, slotIndex, flags, 0);
	}*/

	/// <summary>
	/// can init mult ranges
	/// </summary>
	/// <param name="rangeCount"></param>
	/// <param name="shaderVisibility"></param>
	void DX12RootParameter::InitAsDescriptorTable(UINT rangeCount, const D3D12_DESCRIPTOR_RANGE1* descriptorRangeData, D3D12_SHADER_VISIBILITY shaderVisibility)
	{
		m_rootParamter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		m_rootParamter.ShaderVisibility = shaderVisibility;
		m_rootParamter.DescriptorTable.NumDescriptorRanges = rangeCount;
		m_rootParamter.DescriptorTable.pDescriptorRanges = descriptorRangeData;
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
		UINT slotIndex, UINT rangeIndex, D3D12_DESCRIPTOR_RANGE_FLAGS flags, UINT space)
	{
		D3D12_DESCRIPTOR_RANGE1* range{};
		range->RangeType = rangeType;
		range->NumDescriptors = numDescriptors;
		range->BaseShaderRegister = slotIndex;
		range->RegisterSpace = m_spaceID = space;
		range->OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		range->Flags = flags;
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

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.Desc_1_1.NumParameters = m_numRootParameters;
		rootSignatureDesc.Desc_1_1.pParameters = GetRootParameters();
		rootSignatureDesc.Desc_1_1.NumStaticSamplers = m_numSamplers;
		rootSignatureDesc.Desc_1_1.pStaticSamplers = (const D3D12_STATIC_SAMPLER_DESC*)m_samplerArray.get();
		rootSignatureDesc.Desc_1_1.Flags = flags;
		rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;

		ID3DBlob* signature = nullptr;
		ID3DBlob* error = nullptr;
		ElysiaHelper::ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error));

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