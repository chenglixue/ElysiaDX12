#pragma once
#include "Runtime/Engine/FrameContext.h"

namespace ElysiaRenderer
{
	class IUpdate
	{
	public:
		virtual void Update(const ElysiaEngine::FrameContext& context) = 0;
	};
}