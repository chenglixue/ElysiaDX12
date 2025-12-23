#pragma once

namespace ElysiaCore
{
    struct ShaderMacro
    {
        std::wstring Name;
        std::wstring Value;

        ShaderMacro() = default;
        ShaderMacro(std::wstring n, std::wstring v) : Name(std::move(n)), Value(std::move(v)) {}
    };

    class ShaderCompileOptions
    {
    public:
        ShaderCompileOptions() :
            m_macros(),
            m_includeDirs(),
            m_tempStrings()
        {
            
        }
        void SetShaderPath(const std::wstring& path) {m_path = path;}
        void SetEntry(const std::wstring& entry) { m_entry = entry; }
        void SetTarget(const std::wstring& target) { m_target = target; }

        // --- Macros ---------------------------------------------------
        void AddMacro(const std::wstring& name)
        {
            m_macros.emplace_back(name, L"1");
        }

        void AddMacro(const std::wstring& name, int value)
        {
            m_macros.emplace_back(name, std::to_wstring(value));
        }

        void AddMacro(const std::wstring& name, float value)
        {
            std::wstringstream ss;
            ss << value;
            m_macros.emplace_back(name, ss.str());
        }

        void AddMacro(const std::wstring& name, const std::wstring& value)
        {
            m_macros.emplace_back(name, value);
        }

        // --- Include Paths --------------------------------------------
        void AddIncludeDir(const std::wstring& path)
        {
            m_includeDirs.push_back(path);
        }

        // --- Optimization / Debug -------------------------------------
        void EnableDebug(bool enable)
        {
            m_debug = enable;
        }

        void SetOptLevel(int level) // 0~3
        {
            m_optLevel = level;
        }

        const std::vector<LPCWSTR> BuildArguments() const;

    private:
        std::wstring m_path = L"";
        std::wstring m_entry = L"PS";
        std::wstring m_target = L"ps_6_1";
        bool m_debug = false;
        int m_optLevel = 3;

        std::vector<ShaderMacro> m_macros;
        std::vector<std::wstring> m_includeDirs;

        // Because LPCWSTR must point to persistent strings
        mutable std::vector<std::wstring> m_tempStrings;
    };
}

