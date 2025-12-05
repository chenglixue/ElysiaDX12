#pragma once

namespace ElysiaRenderer
{
	class DX12Device;
}

namespace ElysiaRenderer
{
	class IManager 
	{
	public:
		virtual void Init(DX12Device*) = 0;
		virtual void Destory() = 0;
	};
}
