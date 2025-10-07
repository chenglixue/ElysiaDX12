#pragma once
#include "stdafx.h"
#include "Serialization.h"

namespace ElysiaRenderer
{
	using namespace DirectX::SimpleMath;

	const std::vector<LPCWSTR> g_ModelPaths
	{
		L"Sponza\\Sponza.fbx"
	};

	struct UserData
	{
		Vector3 lightColor = Vector3::One;
		Vector3 lightDir = Vector3::One;
		float lightIntensity = 1.f;

		Vector3 BaseColorTint = Vector3::One;
		float Opacity = 1;
		float NormalIntensity = 1;
		float MetallicIntensity = 1;
		float RoughnessIntensity = 1;
		float AmbientCubemapIntensity = 1;
		Vector3 AmbientCubemapTint = Vector3::One;
	};

	static UserData g_userData;

	inline static void DeSerializeUserData()
	{
		const LPCWSTR filePath = L"D3D12\\UserData.elysia";

		WCHAR assetsPath[512];
		ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));
		auto userDataFullPath = std::filesystem::path(ElysiaHelper::GetAssetFullPath(assetsPath, filePath).c_str()).string();

		if (FileExists(stringToLPCWSTR(userDataFullPath)))
		{
			FileReadSerializer readSerializer(stringToLPCWSTR(userDataFullPath));

			SerializeData(readSerializer, g_userData);
		}
		else
		{
			g_userData = UserData();
		}
	}

	inline static void SerializeUserData()
	{
		const LPCWSTR filePath = L"D3D12\\UserData.elysia";
		WCHAR assetsPath[512];
		ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));
		auto userDataFullPath = std::filesystem::path(ElysiaHelper::GetAssetFullPath(assetsPath, filePath).c_str()).string();

		FileWriteSerializer serializer(stringToLPCWSTR(userDataFullPath));
		SerializeData(serializer, g_userData);
	}
}