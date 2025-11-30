#pragma once
#include "lib/Utility//Helper.h"
#include "MessageID.h"

namespace ElysiaHelper
{
    class Messager
    {
    public:
        using Handler = std::function<void(void*)>;
        
        static Messager& GetInstance()
        {
            std::call_once(m_initInstanceFlag, []() {
                m_instance.reset(new Messager());
                });

            return *m_instance;
        }

        template<typename... T>
        void AddListener(size_t id, std::function<void(const T&...)> listener);

        template<typename... T>
        void Broadcast(size_t id, const T&... value);
        
    private:
        static std::unique_ptr<Messager> m_instance;
        static std::once_flag m_initInstanceFlag;
        
        std::unordered_map<size_t, std::vector<Handler>> m_handlers;
    };

    
}
