#include "stdafx.h"
#include "ShaderKeywordSpace.h"

#include "lib/Utility/Helper.h"

namespace ElysiaRenderer
{
    using namespace ElysiaHelper;
    
    int ShaderKeywordSpace::AddKeyword(const std::wstring& name)
    {
        auto it = m_nameToIndex.find(name);
        
        if(it != m_nameToIndex.end())
        {
            return (int)it->second;
        }
        if(m_nextIndex >= MAX_VARIANTS)
        {
            return -1;
        }

        m_nameToIndex[name] = (UINT8)m_nextIndex;
        m_names.emplace_back(name);

        return (int)m_nextIndex++;
    }

    int ShaderKeywordSpace::GetIndex(const std::wstring& name) const
    {
        auto it = m_nameToIndex.find(name);
        if(it == m_nameToIndex.end())
        {
            return -1;
        }

        return static_cast<int>(it->second);
    }

    const std::wstring& ShaderKeywordSpace::GetName(int index) const
    {
        return m_names.at(index);
    }

    size_t ShaderKeywordSpace::Count() const
    {
        return m_names.size();
    }

    std::vector<std::wstring> ShaderKeywordSpace::GetAllNames() const
    {
        return m_names;
    }
}