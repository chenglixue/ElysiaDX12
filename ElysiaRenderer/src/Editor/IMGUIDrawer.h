#pragma once
#include "Runtime/Core/DX12DescriptorHeapHandle.h"

namespace ElysiaCore
{
    class SwapChain;
    class DX12Device;

    class DX12Context;
}

namespace ElysiaEditor
{
    using namespace ElysiaCore;
    class IMGUIDrawer
    {
    public:
        void OnCreate(DX12Device* pDevice, ElysiaCore::SwapChain* pSwapChain);
        void OnDestory();
        void UpdatePipeline();
        void Draw(DX12Context* pCommand);
        
    private:
        std::vector<DX12DescriptorHeapHandle> m_handles;
    };
}

