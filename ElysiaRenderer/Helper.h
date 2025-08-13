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

namespace ElysiaHelper
{
    struct UINT2
    {
        uint32_t x = 0;
        uint32_t y = 0;
    };
    struct FLOAT2
    {
        float x = 0;
        float y = 0;
    };

    constexpr uint8_t PER_OBJECT_SPACE = 0;
    constexpr uint8_t PER_MATERIAL_SPACE = 1;
    constexpr uint8_t PER_PASS_SPACE = 2;
    constexpr uint8_t PER_FRAME_SPACE = 3;
    constexpr uint8_t NUM_RESOURCE_SPACES = 4;

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

    inline LPCSTR WStringToLPCTSTR(const std::wstring& wstr)
    {
        if (wstr.empty()) return "";

        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);

        return strTo.c_str();
    }

}