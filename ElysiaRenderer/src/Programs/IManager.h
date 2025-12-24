#pragma once
#include "Runtime/Core/DX12Device.h"

namespace ElysiaRenderer
{
	using namespace ElysiaCore;
	class IManager 
	{
	public:
		virtual void Init(DX12Device*) = 0;
		virtual void Destory() = 0;
	};
}
