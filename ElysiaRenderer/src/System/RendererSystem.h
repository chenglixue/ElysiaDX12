#pragma once

#include "lib/DX12/DX12Device.h"
#include "lib/Utility/Helper.h"
#include "src/Pass/BasePass.h"

namespace ElysiaRenderer 
{
	using namespace ElysiaHelper;

	class MeshManager;
	class TextureManager;
	class CameraManager;
	class DX12UI;
	class DX12GraphicsContext;
	struct PipelineStateObject;
	class DX12TextureResource;
	
	class RendererSystem
	{
	public:
		RendererSystem(HWND windowHandle, ElysiaHelper::UINT2 screenSize, DX12UI* pUI);
		~RendererSystem();

		void Init();
		void Update();
		void Render(); 
		void Destory();
		void Resize();

		virtual void OnMouseDown(WPARAM btnState, int x, int y);
		virtual void OnMouseUp(WPARAM btnState, int x, int y);
		virtual void OnMouseMove(WPARAM btnState, int x, int y);
		virtual void OnKeyboardInput();

		bool IsStopped() const
		{
			return m_isStopped;
		}
		void SetIsStopped(bool isStopped)
		{
			m_isStopped = isStopped;
		}
		bool IsMin() const
		{
			return m_isMin;
		}
		void SetIsMin(bool isMin)
		{
			m_isMin = isMin;
		}
		bool IsMax() const
		{
			return m_isMax;
		}
		void SetIsMax(bool isMax)
		{
			m_isMax = isMax;
		}
		bool IsResizing() const
		{
			return m_isResizing;
		}
		void SetIsResizing(bool isResizing)
		{
			m_isResizing = isResizing;
		}

	protected:
		HWND m_windowHandle; 

		bool m_isStopped = false;
		bool m_isMin = false;
		bool m_isMax = false;
		bool m_isResizing = false;

		XMINT2 m_lastMousePos{};
		float m_aspectRatio;

		bool              m_VsyncEnabled;

		// Display management
		DisplayMode               m_currentDisplayMode;
		DisplayMode               m_previousDisplayModeNamesIndex;
		DisplayMode               m_currentDisplayModeNamesIndex;
		std::vector<DisplayMode>  m_displayModesAvailable;
		std::vector<const char*>  m_displayModesNamesAvailable;
		bool                      m_disableLocalDimming;

		std::unique_ptr<DX12Device> m_pDevice = nullptr;
		DX12UI* m_pUI = nullptr;
		std::unique_ptr<DX12GraphicsContext> m_graphicsContext = nullptr;
		std::vector<std::unique_ptr<D3D12_SAMPLER_DESC>> m_samplers{};
		std::vector<std::unique_ptr<BasePass>> m_passes{};

		void Setup();
		void Execute();
		
		void UpdateDisplay(int displayMode, bool disableLocalDimming);
	};     
}   
          