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

    inline void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            throw HrException(hr);
        }
    }

    inline uint32_t AlignU32(uint32_t valueToAlign, uint32_t alignment)
    {
        alignment -= 1;
        return (uint32_t)((valueToAlign + alignment) & ~alignment);
    }

    inline uint64_t AlignU64(uint64_t valueToAlign, uint64_t alignment)
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

    inline std::wstring GetAssetFullPath(std::wstring assetPath, LPCWSTR assetName)
    {
        return assetPath + assetName;
    }

    inline void GetAssetsPath(_Out_writes_(pathSize) WCHAR* path, UINT pathSize)
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

    inline std::string LPCWSTRToString(LPCWSTR wstr) 
    {
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
        std::string str(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &str[0], size, NULL, NULL);
        return str;
    }

    inline std::string GetLastSegmentAfterBackslash(const std::string& str) 
    {
        size_t found = str.rfind('\\');
        if (found != std::string::npos) {
            return str.substr(found + 1);
        }
        return str;
    }

    inline LPCWSTR stringToLPCWSTR(std::string orig)
    {
        size_t origsize = orig.length() + 1;
        const size_t newsize = 100;
        size_t convertedChars = 0;
        wchar_t* wcstring = (wchar_t*)malloc(sizeof(wchar_t) * (orig.length() - 1));
        mbstowcs_s(&convertedChars, wcstring, origsize, orig.c_str(), _TRUNCATE);

        return wcstring;
    }

    inline WCHAR* concatWcharStr(const WCHAR* str1, const WCHAR* str2) {
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

    inline LPCWSTR s2ws(const std::string& s) {
        int len;
        int slength = (int)s.length() + 1;
        len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), slength, 0, 0);
        wchar_t* buf = new wchar_t[len];
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), slength, buf, len);
        LPCWSTR wstr = buf;

        // 注意：调用者需要负责释放内存
        return wstr;
    }

    inline void printWString(LPCWSTR wstr) {
        std::wcout.imbue(std::locale("zh_CN.UTF-8")); // 设置区域以便正确显示 Unicode 字符
        std::wcout << L"Wide string: " << wstr << std::endl;
    }
}