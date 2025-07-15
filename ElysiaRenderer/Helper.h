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

    inline void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            // Set a breakpoint on this line to catch DirectX API errors
            throw std::exception();
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
        if (ppT)
        {
            ppT->Release();
            ppT = nullptr;
        }
    }

    // Helper function for resolving the full path of assets.
    std::wstring GetAssetFullPath(std::wstring assetPath, std::wstring assetName)
    {
        return (assetPath + assetName);
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

    LPCSTR WStringToLPCTSTR(const std::wstring& wstr) 
    {
        if (wstr.empty()) return "";

        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);

        return strTo.c_str();
    }

    struct UINT2
    {
        uint32_t x = 0;
        uint32_t y = 0;
    };
}