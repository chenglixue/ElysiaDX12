#pragma once
#include "stdafx.h"

#include "PSOHelper.h"
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
        UINT2(const Vector2& rhs)
        {
            this->x = static_cast<UINT>(x);
            this->y = static_cast<UINT>(y);
        }
        uint32_t x = 0;
        uint32_t y = 0;
    };
    struct FLOAT2
    {
        float x = 0;
        float y = 0;
    };

    enum EZeroTag { kZero, kOrigin };

    constexpr UINT32 NUM_FRAMES_IN_FLIGHT = 2;
    constexpr UINT32 NUM_BACK_BUFFERS = 3;
    constexpr UINT32 NUM_RTV_STAGING_DESCRIPTORS = 256;
    constexpr UINT32 NUM_DSV_STAGING_DESCRIPTORS = 32;
    constexpr UINT32 NUM_SRV_STAGING_DESCRIPTORS = 4096;
    constexpr UINT32 NUM_SAMPLER_DESCRIPTORS = 8;
    constexpr UINT32 MAX_QUEUED_BARRIERS = 16;
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

    constexpr UINT MAX_MAIN_LIGHT_COUNT = 1;

    using SubResourceLayouts = std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, MAX_TEXTURE_SUBRESOURCE_COUNT>;

    static const wchar_t* SHADER_SOURCE_PATH = L"Shaders/";
    static const wchar_t* SHADER_OUTPUT_PATH = L"Shaders/Complied/";

#define D3D_COMPILE_STANDARD_FILE_INCLUDE ((ID3DInclude*)(UINT_PTR)1)


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
        HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
        HRESULT Error() const { return m_hr; }
    private:
        const HRESULT m_hr;
    };

    inline static void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            _com_error err(hr);
            // 修改 InitD3D 函数中的加载图像部分
            if (FAILED(hr)) 
            {
                std::cerr << "Failed to load image: " << hr << std::endl;
                _com_error err(hr);
            }
            throw HrException(hr);
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
    inline static bool FileExists(const wchar_t* filePath)
    {
        if (filePath == NULL)
            return false;

        DWORD fileAttr = GetFileAttributes(filePath);
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
    inline static std::wstring GetDirectoryFromFilePath(const wchar_t* filePath_)
    {
        assert(filePath_);

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
        if (lastUnderscorePos != std::wstring::npos) {
            // 去除最后一个下划线及其后面的内容
            str.erase(lastUnderscorePos);
        }
        return str;
    }

    inline static std::wstring RemoveLastAnythingAndAfter(std::wstring str, const wchar_t* target)
    {
        // 查找最后一个下划线的位置
        size_t lastUnderscorePos = str.rfind(target);
        if (lastUnderscorePos != std::wstring::npos) {
            // 去除最后一个下划线及其后面的内容
            str.erase(lastUnderscorePos);
        }
        return str;
    }

    inline static std::vector<const char*> StringViewToChar(const std::string_view* stringViewArray, size_t count)
    {
        std::vector<const char*> o{ count };
        for (size_t i = 0; i < count; i++)
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
        std::use_facet<std::ctype<wchar_t>>(std::locale()).narrow(wstr.data(), wstr.data() + wstr.size(), '?', Bufer.data());
        Result = std::string(Bufer.data(), Bufer.size());

        return Result;
    }
}