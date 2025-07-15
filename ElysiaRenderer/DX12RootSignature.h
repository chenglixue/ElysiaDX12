#pragma once
#include "stdafx.h"

namespace ElysiaRenderer
{
	class DX12RootSignature
	{
	public:
		DX12RootSignature();
		~DX12RootSignature();

		ID3D12RootSignature* GetSignature() const
		{
			return m_rootSignature;
		}

	private:
		ID3D12RootSignature* m_rootSignature;
	};
}