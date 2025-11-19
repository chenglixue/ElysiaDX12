#pragma once
#include "stdafx.h"
#include "LightUtility.h"

namespace ElysiaRenderer
{
	using namespace SimpleMath;

	enum class CBVPassParameterType : uint8_t
	{
		Main = 0,
		Shadow = 1
	};

	struct CBVMainPassParameter
	{
		Matrix		viewMatrix			= Matrix::Identity;	// 64
		Matrix		viewMatrix_I		= Matrix::Identity;	// 64
		Matrix		projMatrix			= Matrix::Identity; // 64
		Matrix		projMatrix_I		= Matrix::Identity; // 64
		Matrix		viewProjMatrix		= Matrix::Identity;
		Matrix		viewProjMatrix_I	= Matrix::Identity;
		Vector4		screenSize			= Vector4::Zero;	// 16

		LightData	mainLight;	// 64

		std::array<Vector2, 64> sobolSequence;
	};

	struct CBVObjectParameter
	{
		Matrix	worldMatrix = Matrix::Identity;

		Vector3	baseColorTint = Vector3::One;
		float		opacity = 1.f;

		float		normalIntensity = 1.f;
		float		metallicIntensity = 1.f;
		float		roughnessIntensity = 1.f;
		float		ambientCubemapIntensity = 1.f;

		Vector3	ambientCubemapTint = Vector3::One;
		UINT baseColorTexIndex;

		UINT normalTexIndex;
		UINT metallicTexIndex;
		UINT roughnessTexIndex;
		UINT specularTexIndex;

		float cutoff = 0.5;

		bool hasNormalTex = false;
	};

	struct CBVFrameVariable
	{
		Vector4		cameraPosWS = Vector4::Zero;
		LightData   lightData;

		Matrix		shadowMatrix;
		Vector4		shadowSize;

		Vector4		ZBufferParams;

		UINT		frameIndex = 0;
		float		nearZ = 1;
		float		farZ = 1000;
		UINT		GGX_E_LUT_Index = 0;

		UINT		GGX_Eavg_LUT_Index = 0;
		UINT		SkyboxTexIndex = 0;
		UINT		ShadowTexIndex = 0;
		UINT		BlueNoiseTexIndex = 0;

		UINT		GBuffer0Index = 0;
		UINT		GBuffer1Index = 0;
		UINT		GBuffer2Index = 0;
		UINT		GBuffer3Index = 0;

		UINT		GBuffer4Index = 0;
		UINT		GBuffer5Index = 0;
		UINT		OpaqueDepthIndex = 0;
	};
}