#pragma once
#include "stdafx.h"

namespace ElysiaRenderer
{
	enum class CBVPassParameterType : uint8_t
	{
		Main = 0,
		Shadow = 1
	};

	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)CBVMainPassParameter
	{
		Vector4 cameraPosWS = Vector4::Zero;	// 16
		Matrix viewMatrix = Matrix::Identity;	// 64
		Matrix projMatrix = Matrix::Identity; 	// 64
		Vector4 screenSize = Vector4::Zero;	// 16

		LightData mainLight;	// 64

		UINT frameIndex = 0;
		float nearZ = 1;
		float farZ = 1000;
	};

	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)CBVShadowPassParameter
	{
		XMFLOAT4X4 viewMatrix = MathHelper::Identity4x4();	// 64
		XMFLOAT4X4 projMatrix = MathHelper::Identity4x4(); 	// 64

		XMFLOAT4X4 shadowMatrix = MathHelper::Identity4x4();	// 64

		float nearZ = 1;
		float farZ = 1000;
	};

	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) CBVObjectParameter
	{
		UINT baseColorTexIndex;
		UINT normalTexIndex;
		UINT metallicTexIndex;
		UINT roughnessTexIndex;

		Matrix	worldMatrix = Matrix::Identity;

		Vector3	baseColorTint = Vector3::One;
		float		opacity = 1.f;

		float		normalIntensity = 1.f;
		float		metallicIntensity = 1.f;
		float		roughnessIntensity = 1.f;
		float		ambientCubemapIntensity = 1.f;

		Vector3	ambientCubemapTint = Vector3::One;

		//float padding[48];
	};

	
}