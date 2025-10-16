#pragma once
#include "stdafx.h"
#include "Serialization.h"
#include "DX12Shadow.h"
#include <iostream>
#include <mutex>
#include <memory>

namespace ElysiaRenderer
{
	using namespace DirectX::SimpleMath;

	const std::vector<LPCWSTR> g_ModelPaths
	{
		L"Sponza\\Sponza.fbx"
	};

	class UserData
	{
	public:
		UserData()
		{
			
		}
		UserData(const UserData&) = delete;
		UserData& operator=(const UserData&) = delete;
		UserData(UserData&&) = delete;
		UserData& operator=(UserData&&) = delete;

		static UserData& GetInstance()
		{
			std::call_once(m_initInstanceFlag, []() {
				m_instance.reset(new UserData());
				});

			return *m_instance;
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

		ShadowType shadowType = ShadowType::Soft;
		ShadowQuality shadowQuality = ShadowQuality::VeryHigh;
		float shadowDepthBias = 0;
		float shadowSlopeDepthBias = 0;
		float shadowMaxSlopeDepthBias = 0;

	private:
		static std::unique_ptr<UserData> m_instance;
		static std::once_flag m_initInstanceFlag;
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

			SerializeData(readSerializer, UserData::GetInstance());
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
		SerializeData(serializer, UserData::GetInstance());
	}
}