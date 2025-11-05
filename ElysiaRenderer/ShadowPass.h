#pragma once
#include "BasePass.h"
#include "DX12Shadow.h"
#include "LightManager.h"
#include "RenderTexture.h"


namespace ElysiaRenderer
{
	using namespace ElysiaHelper;

	class ShadowPass : public BasePass
	{
	public:
		ShadowPass() : 
			m_shaderVariables(std::unordered_map<std::string, ShaderVariable>()),
			m_meshResourceLayout(PipelineResourceLayout()),
			m_constantVariableDescs(std::unordered_map<std::string, ShaderConstantVariableDesc>())
		{

		};
		virtual ~ShadowPass() override;

		//virtual void Setup(const RenderPassData& renderPasssData) override;
		virtual void Configure() override;
		virtual void Execute() override;
		virtual void Render() override;

		virtual void Dispose() override;

		RenderTexture* GetShadowRT() const;

	private:
		std::unique_ptr<RenderTexture> m_pShadowRT = nullptr;
		std::unique_ptr<DX12Shadow> m_pMainShadow = nullptr;
		DX12DirectionLight* m_pMainLight = nullptr;

		std::unordered_map<std::string, ShaderVariable> m_shaderVariables;
		std::unordered_map<std::string, ShaderConstantVariableDesc> m_constantVariableDescs;
		PipelineResourceLayout m_meshResourceLayout;

		void CreateMainShadow(float boundSphereRadius, DXGI_FORMAT format);
	};
}