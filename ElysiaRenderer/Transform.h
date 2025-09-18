#pragma once
#include "stdafx.h"

namespace ElysiaUtility
{
	using namespace ElysiaHelper;
	using namespace DirectX::SimpleMath;

	struct Transform
	{
		Vector3 m_position = Vector3::Zero;
		Quaternion m_rotation = Quaternion::Identity;
		Vector3 m_scale = Vector3::One;
	};
}