#pragma once
#include "IManager.h"

namespace ElysiaRenderer
{
	class ShaderManager : public IManager
	{
	public:
		ShaderManager() = default;
		ShaderManager(const ShaderManager& rhs) = delete;
		ShaderManager& operator=(ShaderManager& rhs) = delete;
		ShaderManager(ShaderManager&& rhs) = default;
		~ShaderManager();

		virtual void Init() override;
		virtual void Destory() override;

	private:

	};
}