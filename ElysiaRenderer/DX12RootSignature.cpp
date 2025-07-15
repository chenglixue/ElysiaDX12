#include"DX12RootSignature.h"

namespace ElysiaRenderer
{
	DX12RootSignature::DX12RootSignature()
	{

	}

	DX12RootSignature::DX12RootSignature(ID3D12RootSignature* rootSignature) :
		m_rootSignature(rootSignature)
	{
		
	}

	DX12RootSignature::~DX12RootSignature()
	{
		ElysiaHelper::SafeRelease(m_rootSignature);
	}
}