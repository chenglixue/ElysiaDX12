#pragma once
#include "lib/Utility/ShaderKeywordSet.h"
#include "lib/Utility/ShaderUtility.h"

namespace ElysiaRenderer
{
    class ShaderKeywordSpace;
    class ShaderVariantData;
    
    class ShaderVariantManager
    {
    public:
        using CompileFunc = std::function<ShaderVariantData(const ShaderKeywordSet& keywords)>;
        
        ShaderVariantManager() = default;
        ShaderVariantManager(ShaderKeywordSpace* pKeywordSpace) :
            m_pKeywordSpace(std::move(pKeywordSpace))
        {
            
        }

        void SetCompileCallback(CompileFunc func)
        {
            m_compileFunc = std::move(func);
        }

        std::vector<ShaderVariantData> BuildAllVariants(const ShaderPragmaInfo& pragmaInfo);

        void InitializeFromCompiled(const std::vector<ShaderVariantData> compiledVariants);

        ShaderKeywordSet BuildSetFromNames(const std::vector<std::wstring>& enabledNames) const;

        int FindBestVariantIndex(const ShaderKeywordSet& target);

        const ShaderVariantData& GetOrCompileVariantByNames(const std::vector<std::wstring>& enabledNames);
        const ShaderVariantData& GetOrCompileVariant(const ShaderKeywordSet& set);

        const std::vector<ShaderVariantData>& GetAllVariants() const noexcept;

    private:
        ShaderKeywordSpace* m_pKeywordSpace = nullptr;
        std::vector<ShaderVariantData> m_variants;
        // map from keywordset-key to variant index
        std::unordered_map<ShaderKeywordSet, int, ShaderKeywordSetHash> m_mapKeywordToIndex;

        CompileFunc m_compileFunc;

        void BuildVariantsRecursive(const ShaderPragmaInfo& info,
            size_t groupIndex,
            ShaderKeywordSet& current,
            std::vector<ShaderVariantData>& output);
    };
}

