#pragma once
#include "ThirdParty/FreesyncHDR.h"
#include "AMD/libs/AGS/amd_ags.h"

namespace ElysiaHelper
{
    using namespace CAULDRON_DX12;
    
    class UserMarker
    {
    public:
        UserMarker(ID3D12GraphicsCommandList* commandBuffer, const char* name);
        ~UserMarker();
        static void SetAgsContext(AGSContext* agsContext) {
            m_agsContext = agsContext;
        }

    private:

        static AGSContext*          m_agsContext;
        ID3D12GraphicsCommandList*  m_commandBuffer = nullptr;
    };
}