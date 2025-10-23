#pragma once
#include "BasePass.h"
#include "RenderTexture.h"
#include "DX12Device.h"

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
		std::unique_ptr<RenderTexture> m_depthRT = nullptr;

		std::vector<DX12TextureResource*> GetGBuffers()
		{
			std::vector<DX12TextureResource*> temp{ m_GBufferRTs.size()};
			for (auto& RT : m_GBufferRTs)
			{
				temp.emplace_back(RT->GetTexture());
			}

			return temp;
		}
	};
}