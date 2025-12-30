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
		if (!_CrtCheckMemory()) {__debugbreak(); }
		const LPCWSTR filePath = L"D3D12\\UserData.elysia";

		WCHAR assetsPath[512];
		ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));
		auto userDataFullPath = std::filesystem::path(ElysiaHelper::GetAssetFullPath(assetsPath, filePath).c_str());
		if (!_CrtCheckMemory()) {__debugbreak(); }

		
		if (FileExists(userDataFullPath))
		{
			FileReadSerializer readSerializer(userDataFullPath);
			
			SerializeData(readSerializer, UserData::GetInstance());
		}
		else
		{
			UserData::GetInstance();
		}
	}

	void SerializeUserData()
	{
		const LPCWSTR filePath = L"D3D12\\UserData.elysia";
		WCHAR assetsPath[512];
		ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));
		auto userDataFullPath = std::filesystem::path(ElysiaHelper::GetAssetFullPath(assetsPath, filePath));

		FileWriteSerializer serializer(userDataFullPath);
		SerializeData(serializer, UserData::GetInstance());
	}
}