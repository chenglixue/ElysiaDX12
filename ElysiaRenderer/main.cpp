#include "D3D12Lite.h"
#include "Renderer.h"

//using namespace D3D12Lite;

using namespace ElysiaRenderer;
using namespace DirectX::SimpleMath;

//find path to WinPixGpuCapturer.dll from the most-recently installed version of PIX
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
    switch (umessage)
    {
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE)
        {
            PostQuitMessage(0);
            return 0;
        }
        else
        {
            return DefWindowProc(hwnd, umessage, wparam, lparam);
        }

    case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, umessage, wparam, lparam);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nShowCmd)
{
	std::wstring applicationName = L"Elysia Renderer";
	ElysiaHelper::UINT2 windowSize = { 1920, 1080 };
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

	HWND windowHandle = CreateWindowEx(WS_EX_APPWINDOW, applicationName.c_str(), applicationName.c_str(),
		WS_OVERLAPPEDWINDOW,
		(GetSystemMetrics(SM_CXSCREEN) - windowSize.x) / 2, (GetSystemMetrics(SM_CYSCREEN) - windowSize.y) / 2, windowSize.x, windowSize.y,
		nullptr, nullptr, moduleHandle, nullptr);

	ShowWindow(windowHandle, SW_SHOWMAXIMIZED);
	SetForegroundWindow(windowHandle);
	SetFocus(windowHandle);
	ShowCursor(true);

	GetLatestWinPixGpuCapturerPath_Cpp17();

	std::unique_ptr<ElysiaRenderer::Renderer> renderer = std::make_unique<ElysiaRenderer::Renderer>(windowHandle, windowSize);
	renderer->Init();

	bool shouldExit = false;
	while (!shouldExit)
	{
		MSG msg{ 0 };
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT)
		{
			shouldExit = true;
		}

		renderer->Update();
		renderer->Render();
	}

	DestroyWindow(windowHandle);
	windowHandle = nullptr;

	UnregisterClass(applicationName.c_str(), moduleHandle);
	moduleHandle = nullptr;

	renderer->Destory();
	renderer = nullptr;

	return 0;
}
