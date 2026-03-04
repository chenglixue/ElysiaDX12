#pragma once
#include "stdafx.h"
#include "ThirdParty/Misc.h"

#include "Runtime/Core/PSOHelper.h"
#include "RenderHelper.h"
#include "Math.h"
#include "AMD/libs/vectormath/vectormath.hpp"
#include "ThirdParty/DxgiFormatHelper.h"
#include "Programs/Hash.h"
#include "ThirdParty/stb_image.h"
#include "ThirdParty/DXTex/DDS.h"

namespace ElysiaHelper
{


    enum EZeroTag { kZero, kOrigin };

    constexpr UINT32 NUM_FRAMES_IN_FLIGHT = 2;
    constexpr UINT32 NUM_BACK_BUFFERS = 3;
    constexpr UINT32 NUM_RTV_STAGING_DESCRIPTORS = 1024;
    constexpr UINT32 NUM_DSV_STAGING_DESCRIPTORS = 32;
    constexpr UINT32 NUM_SRV_STAGING_DESCRIPTORS = 32768;
    constexpr UINT32 NUM_SAMPLER_DESCRIPTORS = 8;
    constexpr UINT32 MAX_QUEUED_BARRIERS = 128;
    constexpr UINT8 PER_OBJECT_SPACE = 0;
    constexpr UINT8 PER_MATERIAL_SPACE = 1;
    constexpr UINT8 PER_PASS_SPACE = 2;
    constexpr UINT8 PER_FRAME_SPACE = 3;
    constexpr UINT8 NUM_RESOURCE_SPACES = 4;
    constexpr UINT32 NUM_RESERVED_SRV_DESCRIPTORS = 8192;
    constexpr UINT32 IMGUI_RESERVED_DESCRIPTOR_INDEX = 0;
    constexpr UINT32 NUM_SRV_RENDER_PASS_USER_DESCRIPTORS = 65536;
    constexpr UINT32 INVALID_RESOURCE_TABLE_INDEX = UINT_MAX;
    constexpr UINT32 MAX_TEXTURE_SUBRESOURCE_COUNT = 32;
    constexpr UINT32 StandardMSAAPattern = 0xFFFFFFFF;
    constexpr UINT32 MAX_VARIANTS = 128;

    constexpr UINT MAX_MAIN_LIGHT_COUNT = 256;

    using SubResourceLayouts = std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, MAX_TEXTURE_SUBRESOURCE_COUNT>;

    static const wchar_t* SHADER_SOURCE_PATH = L"Shaders/";
    static const wchar_t* SHADER_OUTPUT_PATH = L"Shaders/Complied/";

#define D3D_COMPILE_STANDARD_FILE_INCLUDE ((ID3DInclude*)(UINT_PTR)1)
#define ArraySize_(x) ((sizeof(x) / sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

#ifdef _DEBUG
#define Elysia_Assert(expression) \
if (!(expression)) { \
printf("Assert failed: %s, file %s, line %d\n", #expression, __FILE__, __LINE__); \
__debugbreak(); \
}
#else
#define Elysia_Assert(expression) ((void)0)
#endif

    inline void AssertIfFailed(HRESULT hr)
    {
        assert(SUCCEEDED(hr));
    }

    inline void AssertError(const char* errorMessage)
    {
        assert((errorMessage, false));
    }

    inline void ShowErrorMessage(const std::wstring& message)
    {
        MessageBox(NULL, message.c_str(), L"Error", MB_OK | MB_ICONERROR);
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

    inline std::string GetWin32ErrorStringAnsi(DWORD errorCode)
    {
        char errorString[MAX_PATH];
        ::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,
                         0,
                         errorCode,
                         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                         errorString,
                         MAX_PATH,
                         NULL);

        std::string message = "Win32 Error: ";
        message += errorString;
        return message;
    }

    class HrException : public std::runtime_error
    {
    public:
        HrException(HRESULT hr)
            : std::runtime_error(HrToString(hr)),
              m_hr(hr)
        {
        }
        HRESULT Error() const
        {
            return m_hr;
        }

    private:
        const HRESULT m_hr;
    };

    inline void ShowErrorMessageBox(LPCWSTR lpErrorString)
    {
        int msgboxID = MessageBoxW(NULL, lpErrorString, L"Error", MB_OK);
    }

    inline static void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            wchar_t err[256];
            memset(err, 0, 256);
            FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM,
                           NULL,
                           hr,
                           MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           err,
                           255,
                           NULL);
            char errA[256];
            size_t returnSize;
            wcstombs_s(&returnSize, errA, 255, err, 255);
            Trace(errA);
#ifdef _DEBUG
            ShowErrorMessageBox(err);
#endif
            throw 1;
        }
    }

    inline static void ThrowIfFailed(BOOL hr)
    {
        if (hr == 0)
        {
            std::cerr << GetWin32ErrorStringAnsi(GetLastError()).c_str();
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

    template <class T>
    void SafeRelease(T& ppT)
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
        if (found != std::string::npos)
        {
            return str.substr(found + 1);
        }
        return str;
    }


    inline static WCHAR* concatWcharStr(const WCHAR* str1, const WCHAR* str2)
    {
        size_t len1 = wcslen(str1) * 2;
        size_t len2 = wcslen(str2) * 2;
        size_t len3 = len1 + len2;
        char* address = (char*)malloc(len3 + 2);
        for (size_t i = 0; i < len1; i ++)
            address[i] = ((char*)str1)[i];
        for (size_t i = len1; i < len3; i ++)
            address[i] = ((char*)str2)[i];

        address[len3] = '\0';
        address[len3 + 1] = '\0';

        return (WCHAR*)address;
    }

    inline static LPCWSTR s2ws(const std::string& s)
    {
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
    inline static std::wstring StringToWstring(const std::string& str)
    {
        if (str.empty())
            return L"";

#ifdef _WIN32
        // 1. 第一次调用：获取转换后所需的有效字符数（不含 \0）
        // 通过传入 (int)str.size() 明确告诉 API 不要处理结尾的空字符
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), NULL, 0);

        if (size_needed <= 0)
            return L"";

        // 2. 预分配空间
        std::wstring wstrTo(size_needed, 0);

        // 3. 第二次调用：执行真正的转换
        MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstrTo[0], size_needed);

        return wstrTo;
#else
        // Linux/macOS 保持不变，但需注意 codecvt 同样不包含 \0
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.from_bytes(str);
#endif
    }

    // Convert std::wstring to std::string
    inline static std::string WstringToString(const std::wstring& wstr)
    {
        if (wstr.empty())
            return "";

        // 方法1：C++11（已弃用，但简单）
#ifdef _WIN32
        // Windows 下用 Win32 API 更可靠
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (size == 0)
            return "";
        std::string str(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, nullptr, nullptr);
        return str;
#else
// Linux/macOS 使用 C++11（需编译器支持）
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.to_bytes(wstr);
#endif
    }

    inline eastl::wstring ToEastlWString(const std::wstring& str)
    {
        return eastl::wstring(str.c_str());
    }

    inline std::wstring ToStdWString(const eastl::wstring& str)
    {
        return std::wstring(str.c_str());
    }

    /**
     * @brief 接受 std::string, char*, 或 string_view 并转为 eastl::string
     */
    inline eastl::string ToEastl(std::string_view sv)
    {
        return eastl::string(sv.data(), sv.length());
    }

    /**
     * @brief 接受 eastl::string (需手动转 view 或重载), char*, std::string 并转为 std::string
     * 注意：EASTL 默认没有到 std::string_view 的隐式转换，所以我们保留一个重载
     */
    inline std::string ToStd(std::string_view sv)
    {
        return std::string(sv.data(), sv.length());
    }

    // 专门针对 eastl::string 的重载，方便直接调用
    inline std::string ToStd(const eastl::string& str)
    {
        return std::string(str.data(), str.length());
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

    inline static std::wstring RemoveExt(const char* filename)
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

    // Returns true if a file exits
    inline static bool FileExists(const std::wstring& filePath)
    {
        if (filePath.c_str() == NULL)
            return false;

        DWORD fileAttr = GetFileAttributes(filePath.c_str());
        if (fileAttr == INVALID_FILE_ATTRIBUTES)
            return false;

        return true;
    }

    // Retursn true if a directory exists
    inline static bool DirectoryExists(const wchar_t* dirPath)
    {
        if (dirPath == NULL)
            return false;

        DWORD fileAttr = GetFileAttributes(dirPath);
        return (fileAttr != INVALID_FILE_ATTRIBUTES && (fileAttr & FILE_ATTRIBUTE_DIRECTORY));
    }

    // Returns the directory containing a file
    inline static std::wstring GetDirectoryFromFilePath(const std::wstring& filePath_)
    {
        assert(filePath_.c_str());

        std::wstring filePath(filePath_);
        size_t idx = filePath.rfind(L'\\');
        if (idx != std::wstring::npos)
            return filePath.substr(0, idx + 1);
        else
            return std::wstring(L"");
    }

    // Returns the name of the file given the path (extension included)
    inline static std::wstring GetFileName(const wchar_t* filePath_)
    {
        assert(filePath_);

        std::wstring filePath(filePath_);
        size_t idx = filePath.rfind(L'\\');
        if (idx != std::wstring::npos && idx < filePath.length() - 1)
            return filePath.substr(idx + 1);
        else
        {
            idx = filePath.rfind(L'/');
            if (idx != std::wstring::npos && idx < filePath.length() - 1)
                return filePath.substr(idx + 1);
            else
                return filePath;
        }
    }

    // Returns the given file path, minus the extension
    inline static std::wstring GetFilePathWithoutExtension(const wchar_t* filePath_)
    {
        assert(filePath_);

        std::wstring filePath(filePath_);
        size_t idx = filePath.rfind(L'.');
        if (idx != std::wstring::npos)
            return filePath.substr(0, idx);
        else
            return std::wstring(L"");
    }

    // Returns the name of the file given the path, minus the extension
    inline static std::wstring GetFileNameWithoutExtension(const wchar_t* filePath)
    {
        std::wstring fileName = GetFileName(filePath);
        return GetFilePathWithoutExtension(fileName.c_str());
    }

    // Returns the extension of the file path
    inline static std::wstring GetFileExtension(const wchar_t* filePath_)
    {
        assert(filePath_);

        std::wstring filePath(filePath_);
        size_t idx = filePath.rfind(L'.');
        if (idx != std::wstring::npos)
            return filePath.substr(idx + 1, filePath.length() - idx - 1);
        else
            return std::wstring(L"");
    }

    // Gets the last written timestamp of the file
    inline static UINT64 GetFileTimestamp(const wchar_t* filePath)
    {
        assert(filePath);

        WIN32_FILE_ATTRIBUTE_DATA attributes;
        ThrowIfFailed(GetFileAttributesEx(filePath, GetFileExInfoStandard, &attributes));
        return attributes.ftLastWriteTime.dwLowDateTime | (UINT64(attributes.ftLastWriteTime.dwHighDateTime) << 32);
    }


    inline static std::wstring RemoveLastUnderscoreAndAfter(std::wstring str)
    {
        // 查找最后一个下划线的位置
        size_t lastUnderscorePos = str.rfind(L'_');
        if (lastUnderscorePos != std::wstring::npos)
        {
            // 去除最后一个下划线及其后面的内容
            str.erase(lastUnderscorePos);
        }
        return str;
    }

    inline static std::wstring RemoveLastAnythingAndAfter(std::wstring str, const wchar_t* target)
    {
        // 查找最后一个下划线的位置
        size_t lastUnderscorePos = str.rfind(target);
        if (lastUnderscorePos != std::wstring::npos)
        {
            // 去除最后一个下划线及其后面的内容
            str.erase(lastUnderscorePos);
        }
        return str;
    }

    inline static std::vector<const char*> StringViewToChar(const std::string_view* stringViewArray, size_t count)
    {
        std::vector<const char*> o{count};
        for (size_t i = 0; i < count; i ++)
        {
            o[i] = stringViewArray[i].data();
        }

        return o;
    }

    // https://gist.github.com/danzek/d6a0e4a48a5439e7f808ed1497f6268e
    inline std::wstring S2W(const std::string& Str)
    {
        std::vector<wchar_t> buf(Str.size());
        std::use_facet<std::ctype<wchar_t>>(std::locale()).widen(Str.data(),
                                                                 Str.data() + Str.size(),
                                                                 buf.data());
        return std::wstring(buf.data(), buf.size());
    }
    inline std::string W2S(const std::wstring& wstr)
    {
        std::string Result;
        std::vector<char> Bufer(wstr.size());
        std::use_facet<std::ctype<wchar_t>>(std::locale()).narrow(wstr.data(),
                                                                  wstr.data() + wstr.size(),
                                                                  '?',
                                                                  Bufer.data());
        Result = std::string(Bufer.data(), Bufer.size());

        return Result;
    }

    const std::unordered_map<DXGI_FORMAT, const char*> formatMap = {
        {DXGI_FORMAT_UNKNOWN, "DXGI_FORMAT_UNKNOWN"},
        {DXGI_FORMAT_R32G32B32A32_TYPELESS, "DXGI_FORMAT_R32G32B32A32_TYPELESS"},
        {DXGI_FORMAT_R32G32B32A32_FLOAT, "DXGI_FORMAT_R32G32B32A32_FLOAT"},
        {DXGI_FORMAT_R32G32B32A32_UINT, "DXGI_FORMAT_R32G32B32A32_UINT"},
        {DXGI_FORMAT_R32G32B32A32_SINT, "DXGI_FORMAT_R32G32B32A32_SINT"},
        {DXGI_FORMAT_R32G32B32_TYPELESS, "DXGI_FORMAT_R32G32B32_TYPELESS"},
        {DXGI_FORMAT_R32G32B32_FLOAT, "DXGI_FORMAT_R32G32B32_FLOAT"},
        {DXGI_FORMAT_R32G32B32_UINT, "DXGI_FORMAT_R32G32B32_UINT"},
        {DXGI_FORMAT_R32G32B32_SINT, "DXGI_FORMAT_R32G32B32_SINT"},
        {DXGI_FORMAT_R16G16B16A16_TYPELESS, "DXGI_FORMAT_R16G16B16A16_TYPELESS"},
        {DXGI_FORMAT_R16G16B16A16_FLOAT, "DXGI_FORMAT_R16G16B16A16_FLOAT"},
        {DXGI_FORMAT_R16G16B16A16_UNORM, "DXGI_FORMAT_R16G16B16A16_UNORM"},
        {DXGI_FORMAT_R16G16B16A16_UINT, "DXGI_FORMAT_R16G16B16A16_UINT"},
        {DXGI_FORMAT_R16G16B16A16_SNORM, "DXGI_FORMAT_R16G16B16A16_SNORM"},
        {DXGI_FORMAT_R16G16B16A16_SINT, "DXGI_FORMAT_R16G16B16A16_SINT"},
        {DXGI_FORMAT_R32G32_TYPELESS, "DXGI_FORMAT_R32G32_TYPELESS"},
        {DXGI_FORMAT_R32G32_FLOAT, "DXGI_FORMAT_R32G32_FLOAT"},
        {DXGI_FORMAT_R32G32_UINT, "DXGI_FORMAT_R32G32_UINT"},
        {DXGI_FORMAT_R32G32_SINT, "DXGI_FORMAT_R32G32_SINT"},
        {DXGI_FORMAT_R32G8X24_TYPELESS, "DXGI_FORMAT_R32G8X24_TYPELESS"},
        {DXGI_FORMAT_D32_FLOAT_S8X24_UINT, "DXGI_FORMAT_D32_FLOAT_S8X24_UINT"},
        {DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS, "DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS"},
        {DXGI_FORMAT_X32_TYPELESS_G8X24_UINT, "DXGI_FORMAT_X32_TYPELESS_G8X24_UINT"},
        {DXGI_FORMAT_R10G10B10A2_TYPELESS, "DXGI_FORMAT_R10G10B10A2_TYPELESS"},
        {DXGI_FORMAT_R10G10B10A2_UNORM, "DXGI_FORMAT_R10G10B10A2_UNORM"},
        {DXGI_FORMAT_R10G10B10A2_UINT, "DXGI_FORMAT_R10G10B10A2_UINT"},
        {DXGI_FORMAT_R11G11B10_FLOAT, "DXGI_FORMAT_R11G11B10_FLOAT"},
        {DXGI_FORMAT_R8G8B8A8_TYPELESS, "DXGI_FORMAT_R8G8B8A8_TYPELESS"},
        {DXGI_FORMAT_R8G8B8A8_UNORM, "DXGI_FORMAT_R8G8B8A8_UNORM"},
        {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB"},
        {DXGI_FORMAT_R8G8B8A8_UINT, "DXGI_FORMAT_R8G8B8A8_UINT"},
        {DXGI_FORMAT_R8G8B8A8_SNORM, "DXGI_FORMAT_R8G8B8A8_SNORM"},
        {DXGI_FORMAT_R8G8B8A8_SINT, "DXGI_FORMAT_R8G8B8A8_SINT"},
        {DXGI_FORMAT_R16G16_TYPELESS, "DXGI_FORMAT_R16G16_TYPELESS"},
        {DXGI_FORMAT_R16G16_FLOAT, "DXGI_FORMAT_R16G16_FLOAT"},
        {DXGI_FORMAT_R16G16_UNORM, "DXGI_FORMAT_R16G16_UNORM"},
        {DXGI_FORMAT_R16G16_UINT, "DXGI_FORMAT_R16G16_UINT"},
        {DXGI_FORMAT_R16G16_SNORM, "DXGI_FORMAT_R16G16_SNORM"},
        {DXGI_FORMAT_R16G16_SINT, "DXGI_FORMAT_R16G16_SINT"},
        {DXGI_FORMAT_R32_TYPELESS, "DXGI_FORMAT_R32_TYPELESS"},
        {DXGI_FORMAT_D32_FLOAT, "DXGI_FORMAT_D32_FLOAT"},
        {DXGI_FORMAT_R32_FLOAT, "DXGI_FORMAT_R32_FLOAT"},
        {DXGI_FORMAT_R32_UINT, "DXGI_FORMAT_R32_UINT"},
        {DXGI_FORMAT_R32_SINT, "DXGI_FORMAT_R32_SINT"},
        {DXGI_FORMAT_R24G8_TYPELESS, "DXGI_FORMAT_R24G8_TYPELESS"},
        {DXGI_FORMAT_D24_UNORM_S8_UINT, "DXGI_FORMAT_D24_UNORM_S8_UINT"},
        {DXGI_FORMAT_R24_UNORM_X8_TYPELESS, "DXGI_FORMAT_R24_UNORM_X8_TYPELESS"},
        {DXGI_FORMAT_X24_TYPELESS_G8_UINT, "DXGI_FORMAT_X24_TYPELESS_G8_UINT"},
        {DXGI_FORMAT_R8G8_TYPELESS, "DXGI_FORMAT_R8G8_TYPELESS"},
        {DXGI_FORMAT_R8G8_UNORM, "DXGI_FORMAT_R8G8_UNORM"},
        {DXGI_FORMAT_R8G8_UINT, "DXGI_FORMAT_R8G8_UINT"},
        {DXGI_FORMAT_R8G8_SNORM, "DXGI_FORMAT_R8G8_SNORM"},
        {DXGI_FORMAT_R8G8_SINT, "DXGI_FORMAT_R8G8_SINT"},
        {DXGI_FORMAT_R16_TYPELESS, "DXGI_FORMAT_R16_TYPELESS"},
        {DXGI_FORMAT_R16_FLOAT, "DXGI_FORMAT_R16_FLOAT"},
        {DXGI_FORMAT_D16_UNORM, "DXGI_FORMAT_D16_UNORM"},
        {DXGI_FORMAT_R16_UNORM, "DXGI_FORMAT_R16_UNORM"},
        {DXGI_FORMAT_R16_UINT, "DXGI_FORMAT_R16_UINT"},
        {DXGI_FORMAT_R16_SNORM, "DXGI_FORMAT_R16_SNORM"},
        {DXGI_FORMAT_R16_SINT, "DXGI_FORMAT_R16_SINT"},
        {DXGI_FORMAT_R8_TYPELESS, "DXGI_FORMAT_R8_TYPELESS"},
        {DXGI_FORMAT_R8_UNORM, "DXGI_FORMAT_R8_UNORM"},
        {DXGI_FORMAT_R8_UINT, "DXGI_FORMAT_R8_UINT"},
        {DXGI_FORMAT_R8_SNORM, "DXGI_FORMAT_R8_SNORM"},
        {DXGI_FORMAT_R8_SINT, "DXGI_FORMAT_R8_SINT"},
        {DXGI_FORMAT_A8_UNORM, "DXGI_FORMAT_A8_UNORM"},
        {DXGI_FORMAT_R1_UNORM, "DXGI_FORMAT_R1_UNORM"},
        {DXGI_FORMAT_R9G9B9E5_SHAREDEXP, "DXGI_FORMAT_R9G9B9E5_SHAREDEXP"},
        {DXGI_FORMAT_R8G8_B8G8_UNORM, "DXGI_FORMAT_R8G8_B8G8_UNORM"},
        {DXGI_FORMAT_G8R8_G8B8_UNORM, "DXGI_FORMAT_G8R8_G8B8_UNORM"},
        {DXGI_FORMAT_BC1_TYPELESS, "DXGI_FORMAT_BC1_TYPELESS"},
        {DXGI_FORMAT_BC1_UNORM, "DXGI_FORMAT_BC1_UNORM"},
        {DXGI_FORMAT_BC1_UNORM_SRGB, "DXGI_FORMAT_BC1_UNORM_SRGB"},
        {DXGI_FORMAT_BC2_TYPELESS, "DXGI_FORMAT_BC2_TYPELESS"},
        {DXGI_FORMAT_BC2_UNORM, "DXGI_FORMAT_BC2_UNORM"},
        {DXGI_FORMAT_BC2_UNORM_SRGB, "DXGI_FORMAT_BC2_UNORM_SRGB"},
        {DXGI_FORMAT_BC3_TYPELESS, "DXGI_FORMAT_BC3_TYPELESS"},
        {DXGI_FORMAT_BC3_UNORM, "DXGI_FORMAT_BC3_UNORM"},
        {DXGI_FORMAT_BC3_UNORM_SRGB, "DXGI_FORMAT_BC3_UNORM_SRGB"},
        {DXGI_FORMAT_BC4_TYPELESS, "DXGI_FORMAT_BC4_TYPELESS"},
        {DXGI_FORMAT_BC4_UNORM, "DXGI_FORMAT_BC4_UNORM"},
        {DXGI_FORMAT_BC4_SNORM, "DXGI_FORMAT_BC4_SNORM"},
        {DXGI_FORMAT_BC5_TYPELESS, "DXGI_FORMAT_BC5_TYPELESS"},
        {DXGI_FORMAT_BC5_UNORM, "DXGI_FORMAT_BC5_UNORM"},
        {DXGI_FORMAT_BC5_SNORM, "DXGI_FORMAT_BC5_SNORM"},
        {DXGI_FORMAT_B5G6R5_UNORM, "DXGI_FORMAT_B5G6R5_UNORM"},
        {DXGI_FORMAT_B5G5R5A1_UNORM, "DXGI_FORMAT_B5G5R5A1_UNORM"},
        {DXGI_FORMAT_B8G8R8A8_UNORM, "DXGI_FORMAT_B8G8R8A8_UNORM"},
        {DXGI_FORMAT_B8G8R8X8_UNORM, "DXGI_FORMAT_B8G8R8X8_UNORM"},
        {DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM, "DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM"},
        {DXGI_FORMAT_B8G8R8A8_TYPELESS, "DXGI_FORMAT_B8G8R8A8_TYPELESS"},
        {DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, "DXGI_FORMAT_B8G8R8A8_UNORM_SRGB"},
        {DXGI_FORMAT_B8G8R8X8_TYPELESS, "DXGI_FORMAT_B8G8R8X8_TYPELESS"},
        {DXGI_FORMAT_B8G8R8X8_UNORM_SRGB, "DXGI_FORMAT_B8G8R8X8_UNORM_SRGB"},
        {DXGI_FORMAT_BC6H_TYPELESS, "DXGI_FORMAT_BC6H_TYPELESS"},
        {DXGI_FORMAT_BC6H_UF16, "DXGI_FORMAT_BC6H_UF16"},
        {DXGI_FORMAT_BC6H_SF16, "DXGI_FORMAT_BC6H_SF16"},
        {DXGI_FORMAT_BC7_TYPELESS, "DXGI_FORMAT_BC7_TYPELESS"},
        {DXGI_FORMAT_BC7_UNORM, "DXGI_FORMAT_BC7_UNORM"},
        {DXGI_FORMAT_BC7_UNORM_SRGB, "DXGI_FORMAT_BC7_UNORM_SRGB"},
        {DXGI_FORMAT_AYUV, "DXGI_FORMAT_AYUV"},
        {DXGI_FORMAT_Y410, "DXGI_FORMAT_Y410"},
        {DXGI_FORMAT_Y416, "DXGI_FORMAT_Y416"},
        {DXGI_FORMAT_NV12, "DXGI_FORMAT_NV12"},
        {DXGI_FORMAT_P010, "DXGI_FORMAT_P010"},
        {DXGI_FORMAT_P016, "DXGI_FORMAT_P016"},
        {DXGI_FORMAT_420_OPAQUE, "DXGI_FORMAT_420_OPAQUE"},
        {DXGI_FORMAT_YUY2, "DXGI_FORMAT_YUY2"},
        {DXGI_FORMAT_Y210, "DXGI_FORMAT_Y210"},
        {DXGI_FORMAT_Y216, "DXGI_FORMAT_Y216"},
        {DXGI_FORMAT_NV11, "DXGI_FORMAT_NV11"},
        {DXGI_FORMAT_AI44, "DXGI_FORMAT_AI44"},
        {DXGI_FORMAT_IA44, "DXGI_FORMAT_IA44"},
        {DXGI_FORMAT_P8, "DXGI_FORMAT_P8"},
        {DXGI_FORMAT_A8P8, "DXGI_FORMAT_A8P8"},
        {DXGI_FORMAT_B4G4R4A4_UNORM, "DXGI_FORMAT_B4G4R4A4_UNORM"},
        {DXGI_FORMAT_P208, "DXGI_FORMAT_P208"},
        {DXGI_FORMAT_V208, "DXGI_FORMAT_V208"},
        {DXGI_FORMAT_V408, "DXGI_FORMAT_V408"},
        {DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE, "DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE"},
        {DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE, "DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE"},
        {DXGI_FORMAT_FORCE_UINT, "DXGI_FORMAT_FORCE_UINT"}
    };

    // 函数：将 DXGI_FORMAT 转换为字符串
    inline const char* DXGIFormatToString(DXGI_FORMAT format)
    {
        auto it = formatMap.find(format);
        if (it != formatMap.end())
        {
            return it->second;
        }
        return "Unknown";
    }

    inline std::wstring FormatHrMessage(HRESULT hr)
    {
        // Try _com_error first
        _com_error err(hr);
        LPCTSTR msg = err.ErrorMessage();
        if (msg && *msg)
            return std::wstring(msg);

        // Fallback to FormatMessage
        LPWSTR buffer = nullptr;
        DWORD size = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            hr,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&buffer,
            0,
            nullptr);

        std::wstring out;
        if (size && buffer)
        {
            out.assign(buffer, buffer + size);
            LocalFree(buffer);
        }
        return out;
    }

    inline bool FloatEqual(float a, float b, float eps = 1e-6f)
    {
        return std::abs(a - b) <= eps;
    }

    inline bool IsFileLocked(const std::wstring& path)
    {
        HANDLE hFile = CreateFileW(
            path.c_str(),
            GENERIC_READ,
            0,
            // 不共享
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
            );

        if (hFile == INVALID_HANDLE_VALUE)
        {
            DWORD error = GetLastError();
            if (error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION)
            {
                return true;
            }
        }
        else
        {
            CloseHandle(hFile);
        }
        return false;
    }

    inline void PrintPathInfo(const std::wstring& path)
    {
        // 打印原始路径
        std::wcout << L"Raw path: " << path << std::endl;

        // 检查路径长度
        std::wcout << L"Path length: " << path.length() << L" characters" << std::endl;

        // 检查路径是否包含特殊字符
        bool hasSpecialChars = false;
        for (wchar_t c : path)
        {
            if (c < 32 || c > 126)
            {
                hasSpecialChars = true;
                std::wcout << L"Special character found: U+" << std::hex << static_cast<int>(c) << std::endl;
            }
        }

        if (!hasSpecialChars)
        {
            std::wcout << L"No special characters found" << std::endl;
        }
    }

    inline bool TestFileAccess(const std::wstring& path)
    {
        // 尝试使用低级API打开文件
        HANDLE hFile = CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
            );

        if (hFile == INVALID_HANDLE_VALUE)
        {
            DWORD error = GetLastError();
            std::wcerr << L"CreateFileW failed with error: " << error << std::endl;
            return false;
        }

        // 获取文件大小
        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(hFile, &fileSize))
        {
            DWORD error = GetLastError();
            std::wcerr << L"GetFileSizeEx failed with error: " << error << std::endl;
            CloseHandle(hFile);
            return false;
        }

        std::wcout << L"File size: " << fileSize.QuadPart << L" bytes" << std::endl;

        // 尝试读取文件头
        BYTE buffer[8];
        DWORD bytesRead;
        if (!ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, NULL))
        {
            DWORD error = GetLastError();
            std::wcerr << L"ReadFile failed with error: " << error << std::endl;
            CloseHandle(hFile);
            return false;
        }

        // 检查PNG文件头 (89 50 4E 47 0D 0A 1A 0A)
        if (bytesRead == 8 &&
            buffer[0] == 0x89 && buffer[1] == 0x50 && buffer[2] == 0x4E && buffer[3] == 0x47 &&
            buffer[4] == 0x0D && buffer[5] == 0x0A && buffer[6] == 0x1A && buffer[7] == 0x0A)
        {
            std::wcout << L"Valid PNG header detected" << std::endl;
        }
        else
        {
            std::wcout << L"Invalid PNG header detected" << std::endl;
        }

        CloseHandle(hFile);
        return true;
    }

    inline std::wstring SanitizePath(const std::wstring& path)
    {
        // 创建不含空字符的副本
        std::wstring sanitized;
        sanitized.reserve(path.size());

        for (wchar_t c : path)
        {
            if (c != L'\0')
            {
                sanitized += c;
            }
        }

        return sanitized;
    }


    inline bool LoadWithSTB(const std::wstring& path, DirectX::ScratchImage& image)
    {
        // 将宽字符串转换为UTF-8
        std::string utf8Path = WstringToString(path);

        // 使用stb_image加载
        int width, height, channels;
        unsigned char* data = stbi_load(utf8Path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

        if (!data)
        {
            std::cerr << "STB failed to load image: " << stbi_failure_reason() << std::endl;
            return false;
        }

        // 创建ScratchImage
        DirectX::TexMetadata metadata;
        metadata.width = width;
        metadata.height = height;
        metadata.depth = 1;
        metadata.arraySize = 1;
        metadata.mipLevels = 1;
        metadata.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        metadata.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;

        HRESULT hr = image.Initialize(metadata);
        if (FAILED(hr))
        {
            stbi_image_free(data);
            return false;
        }

        // 复制像素数据
        const DirectX::Image* img = image.GetImage(0, 0, 0);
        memcpy(img->pixels, data, width * height * 4);

        stbi_image_free(data);
        return true;
    }

    inline void WriteLog(const wchar_t* format, ...)
    {
        wchar_t buffer[1024] = {0};
        va_list args;
        va_start(args, format);
        vswprintf_s(buffer, ArraySize_(buffer), format, args);

        OutputDebugStringW(buffer);
        OutputDebugStringW(L"\n");
    }

    inline void WriteLog(const char* format, ...)
    {
        char buffer[1024] = {0};
        va_list args;
        va_start(args, format);
        vsprintf_s(buffer, ArraySize_(buffer), format, args);

        OutputDebugStringA(buffer);
        OutputDebugStringA("\n");
    }


    inline std::wstring MakeString(const wchar_t* format, ...)
    {
        wchar_t buffer[1024] = {0};
        va_list args;
        va_start(args, format);
        vswprintf_s(buffer, ArraySize_(buffer), format, args);
        return std::wstring(buffer);
    }

    inline std::string MakeString(const char* format, ...)
    {
        char buffer[1024] = {0};
        va_list args;
        va_start(args, format);
        vsprintf_s(buffer, ArraySize_(buffer), format, args);
        return std::string(buffer);
    }

    inline bool IsDebugLayerEnabled(ID3D12Device* pDevice)
    {
#ifdef _DEBUG
        ID3D12DebugDevice* debugDevice = nullptr;
        if (SUCCEEDED(pDevice->QueryInterface(IID_PPV_ARGS(&debugDevice))))
        {
            debugDevice->Release();
            return true;
        }
#endif
        return false;
    }
}

namespace eastl
{
    // 当 EASTL 内部触发断言（如 vector 越界）时，会调用此函数
    inline void AssertionFailure(const char* pExpression)
    {
        // 你可以在这里打断点、记录日志或抛出异常
        printf("EASTL Assertion Failure: %s\n", pExpression);
        __debugbreak(); // 触发调试器中断
    }


}

void* operator new[](size_t size,
                     const char* /*name*/,
                     int /*flags*/,
                     unsigned /*debugFlags*/,
                     const char* /*file*/,
                     int /*line*/);

void* operator new[](size_t size,
                     size_t alignment,
                     size_t alignmentOffset,
                     const char* /*name*/,
                     int /*flags*/,
                     unsigned /*debugFlags*/,
                     const char* /*file*/,
                     int /*line*/);