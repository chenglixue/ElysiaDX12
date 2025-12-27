#pragma once
#include "Programs/Helper.h"

namespace ElysiaCore
{
	inline UINT GetGroupCount(UINT threadCount, UINT groupSize)
	{
		return (threadCount + groupSize - 1) / groupSize;
	}
}