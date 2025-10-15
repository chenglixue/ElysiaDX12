#pragma once
#include "stdafx.h"
#include "Serialization.h"
#include "ShadowManager.h"

namespace ElysiaRenderer
{
	using namespace DirectX::SimpleMath;

	const std::vector<LPCWSTR> g_ModelPaths
	{
		L"Sponza\\Sponza.fbx"
	};

	static std::once_flag singletonFlag;
	class UserData
	{
	public:
		UserData() = default;
		UserData(const UserData&) = delete;
		UserData& operator=(const UserData&) = delete;
		UserData(UserData&&) = delete;
		UserData& operator=(UserData&&) = delete;

		static std::shared_ptr<UserData> GetInstance()
		{
			std::call_once(singletonFlag, [&] 
				{
					g_pUserData = std::shared_ptr<UserData>(new UserData());
				});

			return g_pUserData;
		}

		Vector3 lightColor = Vector3::One;
		Vector3 lightDir = Vector3::One;
		float lightIntensity = 1.f;

		Vector3 BaseColorTint = Vector3::One;
		float Opacity = 1;
		float Cutoff = 0.5;
		float NormalIntensity = 1;
		float MetallicIntensity = 1;
		float RoughnessIntensity = 1;
		float AmbientCubemapIntensity = 1;
		Vector3 AmbientCubemapTint = Vector3::One;

		ShadowQuality shadowQuality;
		float shadowDepthBias = 0;
		float shadowSlopeDepthBias = 0;
		float shadowMaxSlopeDepthBias = 0;

	private:
		static std::shared_ptr<UserData> g_pUserData;
	};

	inline static void DeSerializeUserData()
	{
		const LPCWSTR filePath = L"D3D12\\UserData.elysia";

		WCHAR assetsPath[512];
		ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));
		auto userDataFullPath = std::filesystem::path(ElysiaHelper::GetAssetFullPath(assetsPath, filePath).c_str()).string();

		if (FileExists(stringToLPCWSTR(userDataFullPath)))
		{
			FileReadSerializer readSerializer(stringToLPCWSTR(userDataFullPath));

			SerializeData(readSerializer, *UserData::GetInstance());
		}
		else
		{
			UserData::GetInstance();
		}
	}

	inline static void SerializeUserData()
	{
		const LPCWSTR filePath = L"D3D12\\UserData.elysia";
		WCHAR assetsPath[512];
		ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));
		auto userDataFullPath = std::filesystem::path(ElysiaHelper::GetAssetFullPath(assetsPath, filePath).c_str()).string();

		FileWriteSerializer serializer(stringToLPCWSTR(userDataFullPath));
		SerializeData(serializer, *UserData::GetInstance());
	}
}