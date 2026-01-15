#pragma once
#include "Programs/IManager.h"
#include "BindlessTextureManager.h"
#include "Runtime/Core/DX12TextureBuffer.h"

namespace ElysiaRenderer
{

    class TextureManager : public IManager
    {
    public:
        enum LoadFlags
        {
            Dynamic,
            Resident
        };
        struct Handle
        {
            BindlessTextureManager::TextureHandle textureHandle;
            std::wstring filePath;

            bool IsValid() const
            {
                return textureHandle.IsValid();
            }
            static Handle Invalid()
            {
                return {~0u, UINT_MAX};
            }

            UINT GetResourceHeapIndex() const noexcept
            {
                return textureHandle.resourceHeapIndex;
            }

            UINT64 GetWidth()
            {
                return TextureManager::GetInstance().GetTexture(*this)->GetResourceDesc().Width;
            }

            UINT64 GetHeight()
            {
                return TextureManager::GetInstance().GetTexture(*this)->GetResourceDesc().Height;
            }
        };

    public:
        TextureManager() = default;
        TextureManager(const TextureManager& rhs) = delete;
        TextureManager& operator=(TextureManager& rhs) = delete;
        TextureManager(TextureManager&& rhs) = default;
        ~TextureManager();

        static TextureManager& GetInstance()
        {
            std::call_once(m_initInstanceFlag, []()
            {
                m_instance.reset(new TextureManager());
            });

            return *m_instance;
        }

        virtual void Init(DX12Device* pDevice) override;
        virtual void Destory() override;

        Handle LoadDynamicTexture(const std::wstring& filePath, bool isSRGB = false);
        Handle LoadResidentTexture(const std::wstring& filePath, bool isSRGB = false);
        Handle CreateTexture(const D3D12_RESOURCE_DESC& resourceDesc, TexTypeFlags flag, std::wstring name = L"");

        int GetReferenceCount(const std::wstring& filePath);
        size_t GetTotalManagedCount() noexcept;
        DX12TextureResource* GetTexture(Handle handle) const;
        UINT GetResourceHeapIndex(Handle handle) const;
        void PrintStats();

        void Release(Handle handle);
        void UnloadAllResidents();
        void ShutDown();

        void AddTextureResource(std::unique_ptr<DX12TextureResource> pTextureResource);

    private:
        DX12Device* m_pDevice = nullptr;
        static std::unique_ptr<TextureManager> m_instance;
        static std::once_flag m_initInstanceFlag;

        struct ManagedTexture
        {
            LoadFlags flag;
            BindlessTextureManager::TextureHandle textureHandle;
            std::atomic<int> refCount{1};

            bool IsResident()
            {
                return flag == LoadFlags::Resident;
            }
        };
        std::unique_ptr<BindlessTextureManager> m_pBindlessTextureManager = nullptr;

        std::unordered_map<size_t, std::shared_ptr<ManagedTexture>> m_dynamicTextureMap;
        std::unordered_map<size_t, std::shared_ptr<ManagedTexture>> m_residentTextureMap;
#ifdef DEBUG
        std::unordered_map<std::wstring, std::shared_ptr<ManagedTexture>> m_debugTextureMap;
#endif
        std::mutex m_mutex;

        std::unordered_map<std::string, UINT> m_globalRTIndexs{};
    };
}