#include "stdafx.h"
#include "ShaderKeywordSet.h"

namespace ElysiaCore
{
    bool ShaderKeywordSet::operator==(const ShaderKeywordSet& o) const
    {
        return m_bits == o.Bits();
    }
    
    void ShaderKeywordSet::Set(int index, bool on)
    {
        assert(index >= 0);

        m_bits.set((size_t)index, on);
    }

    void ShaderKeywordSet::Clear()
    {
        m_bits.reset();        
    }

    size_t ShaderKeywordSet::Count() const
    {
        return m_bits.count();
    }

    size_t ShaderKeywordSet::MatchingCount(const ShaderKeywordSet& other) const
    {
        bits_t copy = m_bits & other.m_bits;
        return copy.count();
    }

    const ShaderKeywordSet::bits_t& ShaderKeywordSet::Bits() const
    {
        return m_bits;
    }

    std::string ShaderKeywordSet::ToKey() const
    {
        return m_bits.to_string();
    }
}