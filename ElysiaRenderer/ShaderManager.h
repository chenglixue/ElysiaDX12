#pragma once
#include "IManager.h"
#include "Helper.h"

namespace ElysiaRenderer
{
	using namespace ElysiaHelper;

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

	extern std::unique_ptr<ShaderManager> g_pShaderManager;

	inline ShaderManager* GetShaderManager()
	{
		if (g_pShaderManager == nullptr)
		{
			ThrowRuntimeError("null shader manager");
		}
		return g_pShaderManager.get();
	}
}