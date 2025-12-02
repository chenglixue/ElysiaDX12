#include "stdafx.h"
#include "ShaderVariantManager.h"
#include "lib/Utility/ShaderUtility.h"

namespace ElysiaRenderer
{
    std::vector<ShaderVariantData> ShaderVariantManager::BuildAllVariants(const ShaderPragmaInfo& pragmaInfo)
    {
        std::vector<ShaderVariantData> result{};
        ShaderKeywordSet cur{};

        BuildVariantsRecursive(pragmaInfo, 0, cur, result);

        return result;
    }
    
    void ShaderVariantManager::InitializeFromCompiled(const std::vector<ShaderVariantData> compiledVariants)
    {
        m_variants.clear();
        m_mapKeywordToIndex.clear();

        m_variants = std::move(compiledVariants);

        for(size_t variantIndex = 0; variantIndex < compiledVariants.size(); ++variantIndex)
        {
            m_mapKeywordToIndex.emplace(m_variants[variantIndex].KeywordSet, (int)variantIndex);
        }
    }

    ShaderKeywordSet ShaderVariantManager::BuildSetFromNames(const std::vector<std::wstring>& enabledNames) const
    {
        assert(m_pKeywordSpace);

        ShaderKeywordSet set;

        for(const auto& name : enabledNames)
        {
            int index = m_pKeywordSpace->GetIndex(name);
            
            if(index >= 0)
            {
                set.Set(index, true);
            }
        }

        return set;
    }

    int ShaderVariantManager::FindBestVariantIndex(const ShaderKeywordSet& target)
    {
        int bestIdx = -1;
        int bestScore = -1;

        auto& tb = target.Bits();

        for (int i = 0; i < (int)m_variants.size(); i++)
        {
            auto& vb = m_variants[i].KeywordSet.Bits();

            // 评分：匹配的 bit 数量
            int score = (int)(tb & vb).count();

            if (score > bestScore)
            {
                bestScore = score;
                bestIdx = i;
            }
        }

        return bestIdx;
    }

    const ShaderVariantData& ShaderVariantManager::GetOrCompileVariant(const ShaderKeywordSet& set)
    {
        auto it = m_mapKeywordToIndex.find(set);
        if (it != m_mapKeywordToIndex.end())
        {
            return m_variants[it->second];
        }
        
        // 未编译 → 现在编译
        ShaderVariantData v = m_compileFunc(set);
        v.KeywordSet = set;

        int newIndex = (int)m_variants.size();
        m_variants.push_back(v);

        m_mapKeywordToIndex[set] = newIndex;

        return m_variants.back();
    }

    const ShaderVariantData& ShaderVariantManager::GetOrCompileVariantByNames(const std::vector<std::wstring>& enabledNames)
    {
        ShaderKeywordSet set = BuildSetFromNames(enabledNames);
        return GetOrCompileVariant(set);
    }

    void ShaderVariantManager::BuildVariantsRecursive(const ShaderPragmaInfo& info,
        size_t groupIndex,
        ShaderKeywordSet& currentKeywords,
        std::vector<ShaderVariantData>& result)
    {
        if (groupIndex == info.KeywordGroups.size())
        {
            // 回调编译
            ShaderVariantData v = m_compileFunc(currentKeywords);
            v.KeywordSet = currentKeywords;
            
            result.push_back(std::move(v));
            return;
        }

        const auto& group = info.KeywordGroups[groupIndex];

        for (auto& key : group.Keywords)
        {
            if (key.empty()) // "_" 表示 no keyword
            {
                BuildVariantsRecursive(info, groupIndex + 1, currentKeywords, result);
            }
            else
            {
                int idx = m_pKeywordSpace->GetIndex(key);
                currentKeywords.Set(idx, true);
                BuildVariantsRecursive(info, groupIndex + 1, currentKeywords, result);
                currentKeywords.Set(idx, false);
            }
        }
    }

    const std::vector<ShaderVariantData>& ShaderVariantManager::GetAllVariants() const noexcept
    {
        return m_variants;
    }
}