#pragma once
#include "Runtime/RenderCore/RenderItem.h"

namespace ElysiaEngine
{
	struct FrameContext
	{
		UINT frameID;
		UINT64 frameIndex;
		std::vector<ElysiaRenderer::RenderItem> renderList;
	};
}

