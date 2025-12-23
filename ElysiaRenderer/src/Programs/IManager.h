#pragma once

namespace ElysiaCore
{
	class DX12Device;
}

namespace ElysiaRenderer
{
	class IManager 
	{
	public:
		virtual void Init(ElysiaCore::DX12Device*) = 0;
		virtual void Destory() = 0;
	};
}
