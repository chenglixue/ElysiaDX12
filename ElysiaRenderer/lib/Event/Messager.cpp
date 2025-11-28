#include "stdafx.h"
#include "Messager.h"


namespace ElysiaHelper
{
    std::unique_ptr<Messager> g_messager = nullptr;

    void Messager::RegisterHandler(size_t messageID, std::function<void()> handler)
    {
        m_handlers[messageID] = handler;
    }

    void Messager::HandleMessage(size_t messageID)
    {
        if (m_handlers.contains(messageID))
        {
            m_handlers[messageID]();
        }
        else
        {
            ElysiaHelper::ThrowRuntimeError("没有找到消息 ID: " + std::to_string(messageID) + " 的处理函数。" + "\n");
        }
    }

    template<typename T>
    void Messager::HandleMessage(size_t messageID, T value)
    {
        
    }
    
    template<typename T1, typename T2>
    void Messager::HandleMessage(size_t messageID, T1 value1, T2 value2)
    {
        
    }
    
    template<typename T1, typename T2, typename T3>
    void Messager::HandleMessage(size_t messageID, T1 value1, T2 value2, T3 value3)
    {
        
    }
}
