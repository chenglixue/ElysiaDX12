#pragma once
#include "BasePass.h"
#include "RenderTexture.h"

namespace ElysiaRenderer
{
	class GBufferPass : public BasePass
	{
	public:
		GBufferPass() = default;
		virtual ~GBufferPass() override;

		//virtual void Setup(const RenderPassData& renderPassData) override;
		virtual void Configure() override;
		virtual void Execute() override;
		virtual void Render() override;

		virtual void Dispose() override;

	private:
		std::vector<std::unique_ptr<RenderTexture>> m_GBufferRTs{};
	};
}