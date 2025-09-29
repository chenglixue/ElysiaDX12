#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cstdint>
#include <array>
#include <string>
#include <vector>
#include <mutex>
#include <optional>
#include "SimpleMath/SimpleMath.h"
#include <filesystem>

#include "Definition.h"
#include "RenderHelper.h"

namespace ElysiaHelper
{
    struct UINT2
    {
        UINT2(UINT x, UINT y)
        {
            this->x = x;
            this->y = y;
        }
        uint32_t x = 0;
        uint32_t y = 0;
    };
    struct FLOAT2
    {
        float x = 0;
        float y = 0;
    };

    inline void AssertIfFailed(HRESULT hr)
    {
        assert(SUCCEEDED(hr));
    }

    inline void AssertError(const char* errorMessage)
    {
        assert((errorMessage, false));
    }

    inline void ThrowRuntimeError(std::string output)
    {
        throw std::runtime_error(output);
    }

    inline std::string HrToString(HRESULT hr)
    {
        char s_str[64] = {};
        sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
        return std::string(s_str);
    }

    class HrException : public std::runtime_error
    {
    public:
        HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
        HRESULT Error() const { return m_hr; }
    private:
        const HRESULT m_hr;
    };

    inline static void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            throw HrException(hr);
        }
    }

    inline static uint32_t AlignU32(uint32_t valueToAlign, uint32_t alignment)
    {
        alignment -= 1;
        return (uint32_t)((valueToAlign + alignment) & ~alignment);
    }

    inline static uint64_t AlignU64(uint64_t valueToAlign, uint64_t alignment)
    {
        alignment -= 1;
        return (uint64_t)((valueToAlign + alignment) & ~alignment);
    }

    template <class T> void SafeRelease(T& ppT)
    {
        if (ppT != nullptr)
        {
            ppT->Release();
            ppT = nullptr;
        }
    }
    /*template <class T> void SafeRelease(T* ppT)
    {
        if (ppT)
        {
            ppT->Release();
            ppT = nullptr;
        }
    }*/

    inline static std::wstring GetAssetFullPath(std::wstring assetPath, LPCWSTR assetName)
    {
        return assetPath + assetName;
    }

    inline static void GetAssetsPath(_Out_writes_(pathSize) WCHAR* path, UINT pathSize)
    {
        if (path == nullptr)
        {
            throw std::exception();
        }

        DWORD size = GetModuleFileName(nullptr, path, pathSize);
        if (size == 0 || size == pathSize)
        {
            // Method failed or path was truncated.
            throw std::exception();
        }

        WCHAR* lastSlash = wcsrchr(path, L'\\');
        if (lastSlash)
        {
            *(lastSlash + 1) = L'\0';
        }
    }

    inline static std::string LPCWSTRToString(LPCWSTR wstr)
    {
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
        std::string str(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &str[0], size, NULL, NULL);
        return str;
    }

    inline static std::string GetLastSegmentAfterBackslash(const std::string& str)
    {
        size_t found = str.rfind('\\');
        if (found != std::string::npos) {
            return str.substr(found + 1);
        }
        return str;
    }

    inline static LPCWSTR stringToLPCWSTR(std::string orig)
    {
        size_t origsize = orig.length() + 1;
        const size_t newsize = 100;
        size_t convertedChars = 0;
        wchar_t* wcstring = (wchar_t*)malloc(sizeof(wchar_t) * (orig.length() - 1));
        mbstowcs_s(&convertedChars, wcstring, origsize, orig.c_str(), _TRUNCATE);

        return wcstring;
    }

    inline static WCHAR* concatWcharStr(const WCHAR* str1, const WCHAR* str2) {
        size_t len1 = wcslen(str1) * 2;
        size_t len2 = wcslen(str2) * 2;
        size_t len3 = len1 + len2;
        char* address = (char*)malloc(len3 + 2);
        for (size_t i = 0; i < len1; i++)
            address[i] = ((char*)str1)[i];
        for (size_t i = len1; i < len3; i++)
            address[i] = ((char*)str2)[i];

        address[len3] = '\0';
        address[len3 + 1] = '\0';

        return (WCHAR*)address;
    }

    inline static LPCWSTR s2ws(const std::string& s) {
        int len;
        int slength = (int)s.length() + 1;
        len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), slength, 0, 0);
        wchar_t* buf = new wchar_t[len];
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), slength, buf, len);
        LPCWSTR wstr = buf;

        // 注意：调用者需要负责释放内存
        return wstr;
    }

    // Convert std::string to std::wstring
    inline static std::wstring StringToWstring(const std::string& str) {
        if (str.empty()) return L"";

        // 方法1：C++11（已弃用，但简单）
#ifdef _WIN32
// Windows 下用 Win32 API 更可靠
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
        if (size == 0) return L"";
        std::wstring wstr(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size);
        return wstr;
#else
// Linux/macOS 使用 C++11（需编译器支持）
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.from_bytes(str);
#endif
    }

    // Convert std::wstring to std::string
    inline static std::string WstringToString(const std::wstring& wstr) {
        if (wstr.empty()) return "";

        // 方法1：C++11（已弃用，但简单）
#ifdef _WIN32
// Windows 下用 Win32 API 更可靠
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (size == 0) return "";
        std::string str(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, nullptr, nullptr);
        return str;
#else
// Linux/macOS 使用 C++11（需编译器支持）
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.to_bytes(wstr);
#endif
    }


    inline static std::wstring RemoveExtension(const std::wstring& filePath)
    {
        return filePath.substr(0, filePath.rfind(L"."));
    }

    inline static std::wstring UTF8ToWideString(const std::string& str)
    {
        wchar_t wstr[MAX_PATH];
        if (!MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, str.c_str(), -1, wstr, MAX_PATH))
            wstr[0] = L'\0';
        return wstr;
    }

    inline static inline std::wstring RemoveExt(const char* filename)
    {
        return RemoveExtension(UTF8ToWideString(std::string(filename)));
    }

    inline static void printWString(LPCWSTR wstr) 
    {
        std::wcout.imbue(std::locale("zh_CN.UTF-8")); // 设置区域以便正确显示 Unicode 字符
        std::wcout << L"Wide string: " << wstr << std::endl;
    }

    inline static std::wstring GetBasePath(const std::wstring& filePath)
    {
        size_t lastSlash;
        if ((lastSlash = filePath.rfind(L'/')) != std::wstring::npos)
            return filePath.substr(0, lastSlash + 1);
        else if ((lastSlash = filePath.rfind(L'\\')) != std::wstring::npos)
            return filePath.substr(0, lastSlash + 1);
        else
            return L"";
    }
}