#include "stdafx.h"
#include "UserMarker.h"

namespace ElysiaHelper
{
    AGSContext* UserMarker::m_agsContext = nullptr;

    UserMarker::UserMarker(ID3D12GraphicsCommandList* commandBuffer, const char* name) 
    {
        m_commandBuffer = commandBuffer;

        if (m_agsContext)
            agsDriverExtensionsDX12_PushMarker(m_agsContext, m_commandBuffer, name);

        PIXBeginEvent(m_commandBuffer, 0, name);
    }

    UserMarker::~UserMarker()
    {
        if (m_agsContext)
            agsDriverExtensionsDX12_PopMarker(m_agsContext, m_commandBuffer);

        PIXEndEvent(m_commandBuffer);
    }
}