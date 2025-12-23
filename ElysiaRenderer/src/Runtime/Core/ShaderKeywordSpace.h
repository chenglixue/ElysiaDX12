#pragma once

namespace ElysiaCore
{
    class ShaderKeywordSpace
    {
    public:
        ShaderKeywordSpace() = default;

        int AddKeyword(const std::wstring& name);
        int GetIndex(const std::wstring& name) const;
        const std::wstring& GetName(int index) const;
        size_t Count() const; 
        std::vector<std::wstring> GetAllNames() const;
        
    private:
        std::unordered_map<std::wstring, uint8_t> m_nameToIndex;
        std::vector<std::wstring> m_names;
        size_t m_nextIndex = 0;
    };   
}
