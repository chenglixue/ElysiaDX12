#include "stdafx.h"
#include "Messager.h"

namespace ElysiaHelper
{
    std::once_flag Messager::m_initInstanceFlag;
    std::unique_ptr<Messager> Messager::m_instance;

    template<typename... T>
    void Messager::AddListener(size_t id, std::function<void(const T&...)> listener)
    {
        // 将 typed listener 包装为 void*(tuple) 的函数
        auto wrapper = [listener](void* data)
        {
            using TupleType = std::tuple<T...>;
            TupleType* tup = static_cast<TupleType*>(data);
            std::apply(listener, *tup);   // 解包 tuple 调用
        };

        m_handlers[id].push_back(wrapper);
    }

    template<typename... T>
    void Messager::Broadcast(size_t id, const T&... value)
    {
        auto it = m_handlers.find(id);
        if (it == m_handlers.end()) return;

        // 把参数打包成 tuple
        std::tuple<T...> data(value...);

        for (auto& fn : it->second)
        {
            fn((void*)&data);   // 传递 tuple 指针
        }
    }

    template
    void Messager::Broadcast<int>(size_t id, const int& value);
}
