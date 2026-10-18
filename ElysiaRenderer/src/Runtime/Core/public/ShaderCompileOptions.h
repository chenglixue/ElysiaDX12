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

        // --- Macros / defines (DXC -DNAME=value). Same name overwrites. ---
        void SetDefine(const std::wstring& name);
        void SetDefine(const std::wstring& name, int value);
        void SetDefine(const std::wstring& name, const std::wstring& value);

        void AddMacro(const std::wstring& name)
        {
            SetDefine(name);
        }

        void AddMacro(const std::wstring& name, int value)
        {
            SetDefine(name, value);
        }

        void AddMacro(const std::wstring& name, float value)
        {
            std::wstringstream ss;
            ss << value;
            SetDefine(name, ss.str());
        }

        void AddMacro(const std::wstring& name, const std::wstring& value)
        {
            SetDefine(name, value);
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
#ifdef _DEBUG
        bool m_debug = true;
#else
        bool m_debug = false;
#endif
        int m_optLevel = 3;

        std::vector<ShaderMacro> m_macros;
        std::vector<std::wstring> m_includeDirs;

        // Because LPCWSTR must point to persistent strings
        mutable std::vector<std::wstring> m_tempStrings;
    };

    // UE FShaderCompilerEnvironment::SetDefine for one shader type.
    // These are always-on compile constants (tile size, histogram bins), not
    // permutation keywords. Apply once per ShaderPass before CreateShader.
    class ShaderCompilationEnvironment
    {
    public:
        void SetDefine(std::wstring name);
        void SetDefine(std::wstring name, int value);
        void SetDefine(std::wstring name, std::wstring value);

        void ApplyTo(ShaderCompileOptions& options) const;
        const std::vector<ShaderMacro>& GetDefines() const { return m_defines; }

    private:
        std::vector<ShaderMacro> m_defines;
    };
}

