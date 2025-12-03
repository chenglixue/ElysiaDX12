#include "stdafx.h"
#include "ShaderCompileOptions.h"

#include "../DX12/DX12Shader.h"

namespace ElysiaHelper
{
    const std::vector<LPCWSTR> ShaderCompileOptions::BuildArguments() const
    {
        std::vector<LPCWSTR> args
        {
            m_path.c_str(),
            L"-E", m_entry.c_str(),
            L"-T", m_target.c_str(),
#ifndef DEBUG
            L"-Qstrip_reflect",
#endif
            //L"Qstrip_debug",
            DXC_ARG_PACK_MATRIX_ROW_MAJOR,
            //DXC_ARG_ALL_RESOURCES_BOUND,
            //DXC_ARG_DEBUG,
            //DXC_ARG_SKIP_OPTIMIZATIONS,
        };

        m_tempStrings.clear();
        for (auto& m : m_macros)
        {
            m_tempStrings.emplace_back(L"-D" + m.Name + L"=" + m.Value);
            args.emplace_back(m_tempStrings.back().c_str());
        }

        for(auto& includeDir : m_includeDirs)
        {
            
            m_tempStrings.emplace_back(L"-I" + includeDir);
            args.emplace_back(m_tempStrings.back().c_str());
        }

        if (m_debug)
        {
            args.emplace_back(L"-Zi");
            args.emplace_back(L"-Qembed_debug");
        }

        if (m_optLevel >= 0 && m_optLevel <= 3)
        {
            switch (m_optLevel)
            {
            case 0: args.push_back(L"-Od"); break;
            case 1: args.push_back(L"-O1"); break;
            case 2: args.push_back(L"-O2"); break;
            case 3: args.push_back(L"-O3"); break;
            default: break;
            }
        }

        return args;
    }
}
