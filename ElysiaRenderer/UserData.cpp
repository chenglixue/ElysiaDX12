#include "UserData.h"

namespace ElysiaRenderer
{
	std::once_flag UserData::m_initInstanceFlag;
	std::unique_ptr<UserData> UserData::m_instance;
}