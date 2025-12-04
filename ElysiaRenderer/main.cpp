#define _CRT_SECURE_NO_WARNINGS
#include "src/System/RendererSystem.h"
#include "lib/Model/ModelImporter.h"
#include "lib/DX12/DX12UI.h"
#include "pix3.h"

using namespace ElysiaRenderer;
using namespace std;
using Microsoft::WRL::ComPtr;

static std::unique_ptr<RendererSystem> g_pRenderer = nullptr;

//find path to WinPixGpuCapturer.dll from the most-recently installed version of PIX
static std::wstring GetLatestWinPixGpuCapturerPath_Cpp17();
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hwnd, UINT umessage, WPARAM wparam, LPARAM lparam);

int main()
{
	GetLatestWinPixGpuCapturerPath_Cpp17();

	std::wstring applicationName = L"Elysia Renderer";
	ElysiaHelper::UINT2 windowSize = { static_cast<UINT>(1920), static_cast < UINT>(1080) };
	HINSTANCE moduleHandle = GetModuleHandle(nullptr);

	WNDCLASSEX wc = { 0 };
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = moduleHandle;
	wc.hIcon = LoadIcon(nullptr, IDI_WINLOGO);
	wc.hIconSm = wc.hIcon;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = applicationName.c_str();
	wc.cbSize = sizeof(WNDCLASSEX);
	RegisterClassEx(&wc);

	auto pUI = std::make_unique<DX12UI>();

	HWND windowHandle = CreateWindowEx(WS_EX_APPWINDOW, applicationName.c_str(), applicationName.c_str(),
		WS_OVERLAPPEDWINDOW,
		(GetSystemMetrics(SM_CXSCREEN) - windowSize.x) / 2, (GetSystemMetrics(SM_CYSCREEN) - windowSize.y) / 2, windowSize.x, windowSize.y,
		nullptr, nullptr, moduleHandle, nullptr);

	g_pRenderer = std::make_unique<ElysiaRenderer::RendererSystem>(windowHandle, windowSize, pUI);
	g_pRenderer->Init();

	

	ShowWindow(windowHandle, SW_SHOWNORMAL);
	SetForegroundWindow(windowHandle);
	SetFocus(windowHandle);
	ShowCursor(true);

	bool shouldExit = false;
	while (!shouldExit)
	{
		MSG msg{ 0 };
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			if (!g_pRenderer->IsStopped())
			{
				g_pRenderer->Update();
				g_pRenderer->Render();
			}
			else
			{
				Sleep(100);
			}
		}

		if (msg.message == WM_QUIT)
		{
			shouldExit = true;
		}
	}

	DestroyWindow(windowHandle);
	windowHandle = nullptr;

	UnregisterClass(applicationName.c_str(), moduleHandle);
	moduleHandle = nullptr;

	g_pRenderer->Destory();
	g_pRenderer = nullptr;

	return 0;
}




static std::wstring GetLatestWinPixGpuCapturerPath_Cpp17()
{
	LPWSTR programFilesPath = nullptr;
	SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, NULL, &programFilesPath);

	std::filesystem::path pixInstallationPath = programFilesPath;
	pixInstallationPath /= "Microsoft PIX";

	std::wstring newestVersionFound;

	for (auto const& directory_entry : std::filesystem::directory_iterator(pixInstallationPath))
	{
		if (directory_entry.is_directory())
		{
			if (newestVersionFound.empty() || newestVersionFound < directory_entry.path().filename().c_str())
			{
				newestVersionFound = directory_entry.path().filename().c_str();
			}
		}
	}

	if (newestVersionFound.empty())
	{
		// TODO: Error, no PIX installation found
		ElysiaHelper::AssertError("Error, no PIX installation found");
	}

	return pixInstallationPath / newestVersionFound / L"WinPixGpuCapturer.dll";
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT umessage, WPARAM wparam, LPARAM lparam)
{
	if (ImGui_ImplWin32_WndProcHandler(hwnd, umessage, wparam, lparam))
	{
		return true;
	}
	const ImGuiIO imio = ImGui::GetIO();	// ����ui���в���ʱ������Ⱦ���������岻����Ӱ��(����קuiʱ����ֹ�ƶ�����ӽ�)

	switch (umessage)
	{
		// WM_ACTIVATE is sent when the window is activated or deactivated.  
		// We pause the game when the window is deactivated and unpause it 
		// when it becomes active.  
	case WM_ACTIVATE:
	{
		LOWORD(wparam) == WA_INACTIVE ? g_pRenderer->SetIsStopped(true) : g_pRenderer->SetIsStopped(false);

		break;
	}

	case WM_KEYDOWN:
	{
		if (wparam == VK_ESCAPE)
		{
			PostQuitMessage(0);
			return 0;
		}
		else
		{
			return DefWindowProc(hwnd, umessage, wparam, lparam);
		}
	}

	case WM_DESTROY:
	{
		PostQuitMessage(0);
		return 0;
	}

	case WM_CLOSE:
	{
		PostQuitMessage(0);
		return 0;
	}

	// WM_SIZE is sent when the user resizes the window.  
	case WM_SIZE:
	{
		switch (wparam)
		{
		case SIZE_MINIMIZED:
		{
			g_pRenderer->SetIsStopped(true);
			g_pRenderer->SetIsMin(true);
			g_pRenderer->SetIsMax(false);
			break;
		}
		case SIZE_MAXIMIZED:
		{
			g_pRenderer->SetIsStopped(false);
			g_pRenderer->SetIsMin(false);
			g_pRenderer->SetIsMax(true);
			g_pRenderer->Resize();
			break;
		}
		case SIZE_RESTORED:	// change to normal from min or max
		{
			if (g_pRenderer->IsMin())
			{
				g_pRenderer->SetIsStopped(false);
				g_pRenderer->SetIsMin(false);
				g_pRenderer->Resize();
			}
			else if (g_pRenderer->IsMax())
			{
				g_pRenderer->SetIsStopped(false);
				g_pRenderer->SetIsMax(false);
				g_pRenderer->Resize();
			}
			else if (g_pRenderer->IsResizing())
			{
				// If user is dragging the resize bars, we do not resize 
				// the buffers here because as the user continuously 
				// drags the resize bars, a stream of WM_SIZE messages are
				// sent to the window, and it would be pointless (and slow)
				// to resize for each WM_SIZE message received from dragging
				// the resize bars.  So instead, we reset after the user is 
				// done resizing the window and releases the resize bars, which 
				// sends a WM_EXITSIZEMOVE message.
			}
			else
			{
				g_pRenderer->Resize();
			}

			break;
		}
		}

		break;
	}

	// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
	// Here we reset everything based on the new window dimensions.
	case WM_ENTERSIZEMOVE:
	{
		g_pRenderer->SetIsStopped(false);
		g_pRenderer->SetIsResizing(false);
		g_pRenderer->Resize();

		break;
	}

	// The WM_MENUCHAR message is sent when a menu is active and the user presses 
	// a key that does not correspond to any mnemonic or accelerator key. 
	case WM_MENUCHAR:
	{
		// Don't beep when we alt-enter.
		MAKELRESULT(0, MNC_CLOSE);
		break;
	}

	// Catch this message so to prevent the window becoming too small.
	case WM_GETMINMAXINFO:
	{
		((MINMAXINFO*)lparam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lparam)->ptMinTrackSize.y = 200;
		break;
	}

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
	{
		g_pRenderer->OnMouseDown(wparam, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
		break;
	}

	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
	{
		g_pRenderer->OnMouseUp(wparam, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
		break;
	}

	case WM_MOUSEMOVE:
	{
		if (imio.WantCaptureMouse)
		{
			break;
		}

		g_pRenderer->OnMouseMove(wparam, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));

		break;
	}

	default:
	{
		return DefWindowProc(hwnd, umessage, wparam, lparam);
	}
	}
}

