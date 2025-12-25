// #pragma once
// #include "BasePass.h"
//
// namespace ElysiaRenderer
// {
// 	class RenderTexture;
//
// 	class UIPass : public BasePass
// 	{
// 	public:
// 		UIPass() = default;
// 		virtual ~UIPass() override;
//
// 		//virtual void Setup(const RenderPassData& renderPassData) override;
// 		virtual void Configure() override;
// 		virtual void Execute() override;
// 		virtual void Render() override;
// 		virtual void UpdatePSO() override;
// 		virtual void UpdateVariant() override
// 		{
// 		}
// 		
// 		virtual void Dispose() override;
//
// 	private:
// 		std::unique_ptr<RenderTexture> m_pOpaqueRT = nullptr;
//
// 		void AddUIItems();
// 	};
// }