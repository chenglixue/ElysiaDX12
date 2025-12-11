#include "stdafx.h"
#include "BindlessTextureManager.h"

namespace ElysiaRenderer
{
	std::unique_ptr<BindlessTextureManager> BindlessTextureManager::m_instance;
	std::once_flag BindlessTextureManager::m_initInstanceFlag;
}