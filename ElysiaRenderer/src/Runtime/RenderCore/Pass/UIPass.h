 #pragma once
 #include "BasePass.h"

 namespace ElysiaRenderer
 {
 	class RenderTexture;

 	class UIPass : public BasePass
 	{
 	public:
 		UIPass();
 		virtual ~UIPass() override;

 		virtual void Configure() override;
 		virtual void Render(ElysiaEngine::FrameContext& context) override;
 		virtual void UpdatePSO() override;
 		virtual void UpdateVariant() override
 		{
 		}
 		
 		virtual void Dispose() override;

 	private:
 		std::unique_ptr<RenderTexture> m_pOpaqueRT = nullptr;

 		void AddUIItems();
 	};
 }