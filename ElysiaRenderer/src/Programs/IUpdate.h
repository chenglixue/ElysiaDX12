#pragma once

namespace ElysiaEngine
{
	struct FrameContext;
}

namespace ElysiaRenderer
{
	class IUpdate
	{
	public:
		virtual void Update(const ElysiaEngine::FrameContext& context) = 0;
	};
}