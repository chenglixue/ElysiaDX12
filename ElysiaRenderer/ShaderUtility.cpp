#include "stdafx.h"
#include "ShaderUtility.h"

namespace ElysiaRenderer
{
	DXGI_FORMAT MaskToFormat(const uint32_t Mask)
	{
		switch (Mask)
		{
		case 1: // 0001: Only the first component is used (e.g., x or R).
			return DXGI_FORMAT_R32_FLOAT;
		case 3: // 0011: First and second components are used (e.g., xy or RG).
			return DXGI_FORMAT_R32G32_FLOAT;
		case 7: // 0111: First, second, and third components are used (e.g., xyz or RGB).
			return DXGI_FORMAT_R32G32B32_FLOAT;
		case 15: // 1111: All four components are used (e.g., xyzw or RGBA).
			return DXGI_FORMAT_R32G32B32A32_FLOAT;
			// Add more cases here if you're handling other types of data (e.g., integers or 16-bit floats).
		default:
			return DXGI_FORMAT_UNKNOWN;
		}
	}
}