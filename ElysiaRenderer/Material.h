#pragma once
#include "stdafx.h"

namespace ElysiaRenderer
{
	using namespace SimpleMath;

	struct Material
	{
		Color diffuse;
		Color emission;
		Color specular;
		Color ambient;
		float opacity;
		float shininess;
		float specularIntensity;

		enum { maxTexPath = 128 };
		enum { texCount = 6 };
		char texDiffusePath[maxTexPath];
		char texSpecularPath[maxTexPath];
		char texEmissionPath[maxTexPath];
		char texNormalPath[maxTexPath];
		char texLightmapPath[maxTexPath];
		char texReflectionPath[maxTexPath];

		enum { maxMaterialName = 128 };
		char name[maxMaterialName];
	};
}