#include "stdafx.h"

#include "UserData.h"
#include "Serialization.h"

namespace ElysiaRenderer
{
	std::once_flag UserData::m_initInstanceFlag;
	std::unique_ptr<UserData> UserData::m_instance;

	void DeSerializeUserData()
	{
		const LPCWSTR filePath = L"D3D12\\UserData.elysia";

		WCHAR assetsPath[512];
		ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));
		auto userDataFullPath = std::filesystem::path(ElysiaHelper::GetAssetFullPath(assetsPath, filePath).c_str()).string();

		if (FileExists(stringToLPCWSTR(userDataFullPath)))
		{
			FileReadSerializer readSerializer(stringToLPCWSTR(userDataFullPath));

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
		auto userDataFullPath = std::filesystem::path(ElysiaHelper::GetAssetFullPath(assetsPath, filePath).c_str()).string();

		FileWriteSerializer serializer(stringToLPCWSTR(userDataFullPath));
		SerializeData(serializer, UserData::GetInstance());
	}
}