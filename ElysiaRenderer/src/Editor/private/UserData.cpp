#include "stdafx.h"

#include "../public/UserData.h"
#include "../public/ConfigCache.h"

namespace ElysiaRenderer
{
    std::once_flag UserData::m_initInstanceFlag;
    std::unique_ptr<UserData> UserData::m_instance;
    std::vector<std::wstring> g_ModelPaths;

    void DeSerializeUserData()
    {
        ConfigCache::Get().LoadHierarchy();
        ConfigCache::Get().Apply();
    }

    void SerializeUserData()
    {
        ConfigCache::Get().SaveDiff();
    }
}
