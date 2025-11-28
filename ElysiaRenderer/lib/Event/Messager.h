#pragma once
#include "lib/Utility//Helper.h"
#include "MessageID.h"

namespace ElysiaHelper
{
    class Messager
    {
    public:
        void RegisterHandler(size_t messageID, std::function<void()> handler);
        
        void HandleMessage(size_t messageID);
        template<typename T>
        void HandleMessage(size_t messageID, T value);
        template<typename T1, typename T2>
        void HandleMessage(size_t messageID, T1 value1, T2 value2);
        template<typename T1, typename T2, typename T3>
        void HandleMessage(size_t messageID, T1 value1, T2 value2, T3 value3);
        
    private:
        std::unordered_map<size_t, std::function<void()>> m_handlers;
    };

    extern std::unique_ptr<Messager> g_messager;
    Messager* GetMessager()
    {
        return g_messager.get();
    }
}
