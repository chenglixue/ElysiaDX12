#pragma once
#include "Helper.h"
#include "MObject.h"

namespace ElysiaRenderer
{
	class DX12Material : MObject
	{
	protected:
		virtual void Dispose() override;

	public:
		DX12Material() : MObject() {};


	};
}