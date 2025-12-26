#pragma once

namespace ElysiaCore
{
    class DX12Device;

    class DX12Context;
}

namespace ElysiaEditor
{
    using namespace ElysiaCore;
    class IMGUI
    {
    public:
        void OnCreate(HWND windowHandle, DX12Device* pDevice);
        void OnDestory();
        void UpdatePipeline();
        void Draw(DX12Context* pCommand);
        
    private:
        
    };
}

