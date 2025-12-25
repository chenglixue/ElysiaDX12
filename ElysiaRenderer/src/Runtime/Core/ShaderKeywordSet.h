#pragma once
#include "Programs/Helper.h"

namespace ElysiaCore
{
    using namespace ElysiaHelper;
    
    class ShaderKeywordSet
    {
    public:
        using bits_t = std::bitset<MAX_VARIANTS>;

        ShaderKeywordSet() = default;
        bool operator==(const ShaderKeywordSet& o) const;

        void Set(int index, bool on = true);
        void Clear();
        size_t Count() const;
        size_t MatchingCount(const ShaderKeywordSet& other) const;
        const bits_t& Bits() const;
        std::string ToKey() const;
        
    private:
        bits_t m_bits;
    };

    struct ShaderKeywordSetHash 
    {
        size_t operator()(ShaderKeywordSet const& s) const noexcept
        {
            // Hash the string key — fine for 128 bits.
            return std::hash<std::string>()(s.ToKey());
        }
    };
}
