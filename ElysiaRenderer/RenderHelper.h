#pragma once
#include "stdafx.h"

namespace ElysiaHelper
{
	using namespace DirectX;

	inline XMMATRIX GetMVP(XMMATRIX translate, XMMATRIX scale, XMMATRIX rotate)
	{
		return XMMatrixMultiply(rotate, XMMatrixMultiply(translate, scale));
	};
}
