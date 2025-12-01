#include "stdafx.h"
#include "ShaderUtility.h"

#include "Hash.h"

namespace ElysiaRenderer
{
	DXGI_FORMAT MaskToFormat(const uint32_t Mask)
	{
		switch (Mask)
		{
		case 1: // 0001: Only the first component is used (e.g., x or R).
			return DXGI_FORMAT_R32_FLOAT;
		case 3: // 0011: First and second components are used (e.g., xy or RG).
			return DXGI_FORMAT_R32G32_FLOAT;
		case 7: // 0111: First, second, and third components are used (e.g., xyz or RGB).
			return DXGI_FORMAT_R32G32B32_FLOAT;
		case 15: // 1111: All four components are used (e.g., xyzw or RGBA).
			return DXGI_FORMAT_R32G32B32A32_FLOAT;
			// Add more cases here if you're handling other types of data (e.g., integers or 16-bit floats).
		default:
			return DXGI_FORMAT_UNKNOWN;
		}
	}

	bool IsUnderlineKeyword(const std::wstring& s)
	{
		// "_" "__" "___" 都视为空 keyword
		return !s.empty() && std::all_of(s.begin(), s.end(), [](wchar_t c) { return c == L'_'; });
	}

	std::unordered_map<std::wstring, std::wstring> ParseShaderRenderPragmas(const std::wstring& source)
	{
		std::unordered_map<std::wstring, std::wstring> o{};

		std::wregex re(LR"(#\s*pragma\s+(Rasterizer|Blend|Depth)\s+(.*))");
		auto begin = std::wsregex_iterator(source.begin(), source.end(), re);
		auto end = std::wsregex_iterator();

		for (auto it = begin; it != end; ++it)
		{
			std::wstring type = (*it)[1].str(); // Rasterizer / Blend / Depth
			std::wstring args = (*it)[2].str(); // RasterizerBackFaceCull

			o[type] = args;
		}

		return o;
	}

	ShaderPragmaInfo ParseShaderPragmas(const std::wstring& source)
	{
		ShaderPragmaInfo pragmaInfo{};

		std::wregex re(LR"(#\s*pragma\s+([A-Za-z_][A-Za-z0-9_]*)\s+(.*))");
		auto begin = std::wsregex_iterator(source.begin(), source.end(), re);
		auto end = std::wsregex_iterator();

		for (auto it = begin; it != end; ++it)
		{  
			std::wstring type = (*it)[1].str(); 
			std::wstring args = (*it)[2].str();

			PragmaKeywordGroup keywordGroup{};

			std::wistringstream iss(args);
			std::wstring key;
			while (iss >> key)
			{
				// "_" 或 "__" 表示空宏（不定义任何宏）
				if (IsUnderlineKeyword(key))
					keywordGroup.Keywords.push_back(L""); // empty macro
				else
					keywordGroup.Keywords.push_back(key);
			}
			if (!keywordGroup.Keywords.empty())
				pragmaInfo.KeywordGroups.push_back(std::move(keywordGroup));
		}

		return pragmaInfo;
	}

	
}
