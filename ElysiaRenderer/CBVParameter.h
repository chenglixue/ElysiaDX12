#pragma once
#include "stdafx.h"
#include "DX12Light.h"

namespace ElysiaRenderer
{
	using namespace DirectX::SimpleMath;

	enum class CBVPassParameterType : uint8_t
	{
		Main = 0,
		Shadow = 1
	};

	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)CBVMainPassParameter
	{
		Vector4 cameraPosWS		= Vector4::Zero;	// 16
		Matrix	viewMatrix		= Matrix::Identity;	// 64
		Matrix	projMatrix		= Matrix::Identity; 	// 64
		Matrix	shadowMatrix	= Matrix::Identity;	// 64
		Vector4 screenSize		= Vector4::Zero;	// 16
		Vector4 shadowSize		= Vector4::Zero;	// 16

		LightData mainLight;	// 64

		UINT frameIndex = 0;
		float nearZ = 1;
		float farZ = 1000;
		float shadowNearZ = 1;
		float shadowFarZ = 1000;

		int GGX_E_LUT_Index = -1;
		int GGX_Eavg_LUT_Index = -1;
		int SkyboxTexIndex = -1;
		int ShadowTexIndex = -1;
	};

	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) CBVObjectParameter
	{
		Matrix	worldMatrix = Matrix::Identity;

		Vector3	baseColorTint = Vector3::One;
		float		opacity = 1.f;

		float		normalIntensity = 1.f;
		float		metallicIntensity = 1.f;
		float		roughnessIntensity = 1.f;
		float		ambientCubemapIntensity = 1.f;

		Vector3	ambientCubemapTint = Vector3::One;

		int baseColorTexIndex = -1;
		int normalTexIndex = -1;
		int metallicTexIndex = -1;
		int roughnessTexIndex = -1;
		//int vertexIndex = -1;

		int specularTexIndex = -1;
		//Vector3 padding;

		//float padding[48];
	};

	
}