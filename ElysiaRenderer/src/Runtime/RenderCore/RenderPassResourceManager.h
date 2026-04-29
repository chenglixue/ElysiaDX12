#pragma once
#include "TextureManager.h"
#include "Programs/Helper.h"
#include "Programs/IManager.h"
#include "Runtime/Core/BufferUtility.h"

namespace ElysiaCore
{
    class DX12Device;
}

namespace ElysiaRenderer
{
    class RenderTexture;
}

namespace ElysiaRenderer
{
    struct ShaderGlobalData
    {
        TextureManager::Handle skyboxTex;
        TextureManager::Handle GGX_E_LUT_Index;
        TextureManager::Handle GGX_Eavg_LUT_Index;
        TextureManager::Handle blueNoiseTexIndex;
    };
    struct EnvironmentData
    {
        // RenderTexture* pBRDFLUT = nullptr;
        BufferHandle pSHCoefficientsBuffer = nullptr;
    };
    struct SubsurfaceScatterData
    {
        RenderTexture* pPreIntegrateSSSLUT = nullptr;
        RenderTexture* pNDFLUT = nullptr;
    };

    class RenderPassResourceManager : public IManager
    {
    public:
        RenderPassResourceManager();
        RenderPassResourceManager(DX12Device* pDevice);
        RenderPassResourceManager(const RenderPassResourceManager& rhs) = delete;
        RenderPassResourceManager& operator=(RenderPassResourceManager& rhs) = delete;
        RenderPassResourceManager(RenderPassResourceManager&& rhs) = default;
        ~RenderPassResourceManager();

        static RenderPassResourceManager& GetInstance()
        {
            std::call_once(m_initInstanceFlag,
                           []()
                           {
                               m_instance.reset(new RenderPassResourceManager());
                           });

            return *m_instance;
        }

        virtual void Init(DX12Device* pDevice) override;
        virtual void Destory() override;

        // 构造并存储数据
        template <typename T>
        void Create(T* pData)
        {
            if (!pData)
            {
                throw std::invalid_argument("Cannot register a null pointer to Blackboard");
            }

            size_t typeHash = typeid(T).hash_code();

            m_DataMap[typeHash] = static_cast<void*>(pData);
        }

        // 获取数据引用
        template <typename T>
        T& Get()
        {
            size_t typeHash = typeid(T).hash_code();
            auto it = m_DataMap.find(typeHash);

            if (it != m_DataMap.end())
            {
                // 内部自动把 void* 安全地强转回 T* 并解引用
                return *static_cast<T*>(it->second);
            }
            throw std::runtime_error("Resource type not found in Blackboard");
        }

    private:
        DX12Device* m_pDevice = nullptr;
        static std::unique_ptr<RenderPassResourceManager> m_instance;
        static std::once_flag m_initInstanceFlag;

        struct BlackboardData
        {
            virtual ~BlackboardData() = default;
        };
        template <typename T>
        struct TBlackboardData : public BlackboardData
        {
            T Data;
            template <typename... Args>
            TBlackboardData(Args&&... args)
                : Data(std::forward<Args>(args)...)
            {
            }
        };
        std::unordered_map<size_t, void*> m_DataMap;
    };
}