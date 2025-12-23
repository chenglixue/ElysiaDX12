#pragma once
#include "Helper.h"

namespace ElysiaCore
{
	inline size_t GetGroupCount(size_t threadCount, size_t groupSize)
	{
		return (threadCount + groupSize - 1) / groupSize;
	}
}