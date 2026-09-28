#pragma once
#include "Helper.h"

namespace ElysiaHelper
{
    class SBTHelper
    {
    public:
        // 每一个记录包含：ShaderID (32字节) + 可选的本地参数
        struct ShaderRecord
        {
            ShaderRecord(void* pId, uint32_t idSize, void* pData = nullptr, uint32_t dataSize = 0)
                : shaderId(pId),
                  shaderIdSize(idSize),
                  localData(pData),
                  localDataSize(dataSize)
            {
            }

            void* shaderId;
            uint32_t shaderIdSize;
            void* localData;
            uint32_t localDataSize;
        };

        void AddRayGen(void* pId, void* pData = nullptr, uint32_t dataSize = 0);
        void AddMiss(void* pId, void* pData = nullptr, uint32_t dataSize = 0);
        void AddHitGroup(void* pId, void* pData = nullptr, uint32_t dataSize = 0);

        // 构建并上传到 GPU
        void Build(ID3D12Device* pDevice, ID3D12StateObject* pPSO);

        // 获取 DispatchRays 需要的描述符
        D3D12_GPU_VIRTUAL_ADDRESS_RANGE GetRayGenRange() const;
        D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE GetMissRange() const;
        D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE GetHitGroupRange() const;

        void Clear();

    private:
        uint32_t Align(uint32_t size, uint32_t alignment);

        std::vector<ShaderRecord> m_rayGen;
        std::vector<ShaderRecord> m_miss;
        std::vector<ShaderRecord> m_hitGroup;

        Microsoft::WRL::ComPtr<ID3D12Resource> m_sbtBuffer;

        uint32_t m_rayGenSectionSize = 0;
        uint32_t m_missSectionSize = 0;
        uint32_t m_hitGroupSectionSize = 0;

        uint32_t m_rayGenEntrySize = 0;
        uint32_t m_missEntrySize = 0;
        uint32_t m_hitGroupEntrySize = 0;
    };
}