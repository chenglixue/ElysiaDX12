#include "stdafx.h"

#include "UserData.h"
#include "Runtime/Resource/Serialization.h"


namespace ElysiaRenderer
{
    using namespace ElysiaHelper;

    std::once_flag UserData::m_initInstanceFlag;
    std::unique_ptr<UserData> UserData::m_instance;

    void DeSerializeUserData()
    {
        const LPCWSTR filePath = L"D3D12\\UserData.elysia";
        WCHAR assetsPath[512];
        ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));
        auto userDataFullPath = std::filesystem::path(ElysiaHelper::GetAssetFullPath(assetsPath, filePath).c_str());

        std::ifstream i(userDataFullPath);

        // 检查文件是否打开成功
        if (!i.is_open())
            return;
        // 检查文件是否为空
        if (i.peek() == std::ifstream::traits_type::eof())
            return;

        json j;
        try
        {
            i >> j;
        }
        catch (const json::parse_error& e)
        {
            // 捕获到解析错误！
            // 意味着文件损坏、格式不对
            return;
        }

        // 自动完成所有成员的安全赋值
        // 它会自动调用宏生成的 from_json，并且缺少的字段会自动保留 C++ 里的默认值。
        j.get_to(UserData::GetInstance());
    }

    void SerializeUserData()
    {

        auto& data = UserData::GetInstance();

        const LPCWSTR filePath = L"D3D12\\UserData.elysia";
        WCHAR assetsPath[512];
        ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));
        auto userDataFullPath = std::filesystem::path(ElysiaHelper::GetAssetFullPath(assetsPath, filePath));

        auto tempFilePath = userDataFullPath;
        tempFilePath += L".tmp";

        try
        {
            // 🌟 2. 先把数据写入临时文件
            std::ofstream o(tempFilePath);
            if (!o.is_open())
            {
                // 如果连临时文件都建不出来（比如C盘满了/权限不够），直接放弃保存，保护原文件
                return;
            }

            json j = UserData::GetInstance();
            o << j.dump(4);

            // 🌟 3. 必须手动 close，释放文件句柄，否则下面无法进行替换！
            o.close();

            // 🌟 4. 原子替换 (Atomic Replace)
            // 将写好的 .tmp 文件重命名为原文件。这在系统底层是极其快速且安全的。
            std::error_code ec;
            std::filesystem::rename(tempFilePath, userDataFullPath, ec);

            // 如果重命名失败（可能原文件被只读锁定了），尝试用 copy_file + remove 作为 fallback
            if (ec)
            {
                std::filesystem::copy_file(tempFilePath,
                                           userDataFullPath,
                                           std::filesystem::copy_options::overwrite_existing,
                                           ec);
                std::filesystem::remove(tempFilePath, ec);
            }
        }
        catch (...)
        {
            // 捕捉任何 JSON 序列化或文件系统异常，确保哪怕出错也不破坏原来的 UserData.elysia
        }
    }
}