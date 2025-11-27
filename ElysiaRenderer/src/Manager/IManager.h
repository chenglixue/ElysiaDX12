#pragma once

namespace ElysiaRenderer
{
	class IManager 
	{
	public:
		virtual void Init() = 0;
		virtual void Destory() = 0;
	};
}