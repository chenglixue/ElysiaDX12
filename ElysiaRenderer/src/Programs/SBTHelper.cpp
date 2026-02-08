#include "stdafx.h"
#include "SBTHelper.h"

namespace ElysiaHelper
{
    uint32_t SBTHelper::Align(uint32_t size, uint32_t alignment)
    {
        return (size + (alignment - 1)) & ~(alignment - 1);
    }

    void SBTHelper::AddRayGen(void* pId, void* pData, uint32_t dataSize)
    {
        m_rayGen.emplace_back(pId, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, pData, dataSize);
    }

    void SBTHelper::AddMiss(void* pId, void* pData, uint32_t dataSize)
    {
        m_miss.emplace_back(pId, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, pData, dataSize);
    }

    void SBTHelper::AddHitGroup(void* pId, void* pData, uint32_t dataSize)
    {
        m_hitGroup.emplace_back(pId, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, pData, dataSize);
    }

    void SBTHelper::Build(ID3D12Device* pDevice, ID3D12StateObject* pPSO)
    {
        // 1. 计算每个 Entry 的大小（ID + Data，并按 32 字节对齐）
        auto CalculateEntrySize = [&](const std::vector<ShaderRecord>& records)
        {
            uint32_t maxDataSize = 0;
            for (const auto& r : records)
                maxDataSize = std::max(maxDataSize, r.localDataSize);
            return Align(
                D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + maxDataSize,
                D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
        };

        m_rayGenEntrySize = CalculateEntrySize(m_rayGen);
        m_missEntrySize = CalculateEntrySize(m_miss);
        m_hitGroupEntrySize = CalculateEntrySize(m_hitGroup);

        // 2. 计算各 Section 总大小
        m_rayGenSectionSize = Align(m_rayGenEntrySize * (uint32_t)m_rayGen.size(),
                                    D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
        m_missSectionSize = Align(m_missEntrySize * (uint32_t)m_miss.size(),
                                  D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
        m_hitGroupSectionSize = Align(m_hitGroupEntrySize * (uint32_t)m_hitGroup.size(),
                                      D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

        uint32_t totalSize = m_rayGenSectionSize + m_missSectionSize + m_hitGroupSectionSize;

        // 3. 创建上传堆 Buffer
        auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(totalSize);
        pDevice->CreateCommittedResource(&heapProps,
                                         D3D12_HEAP_FLAG_NONE,
                                         &bufferDesc,
                                         D3D12_RESOURCE_STATE_GENERIC_READ,
                                         nullptr,
                                         IID_PPV_ARGS(&m_sbtBuffer));

        // 4. 拷贝数据
        uint8_t* pData;
        m_sbtBuffer->Map(0, nullptr, (void**)&pData);

        auto FillSection = [&](uint8_t* dest,
                               const std::vector<ShaderRecord>& records,
                               uint32_t entrySize)
        {
            for (const auto& r : records)
            {
                memcpy(dest, r.shaderId, r.shaderIdSize);
                if (r.localData)
                    memcpy(dest + r.shaderIdSize, r.localData, r.localDataSize);
                dest += entrySize;
            }
        };

        FillSection(pData, m_rayGen, m_rayGenEntrySize);
        FillSection(pData + m_rayGenSectionSize, m_miss, m_missEntrySize);
        FillSection(pData + m_rayGenSectionSize + m_missSectionSize,
                    m_hitGroup,
                    m_hitGroupEntrySize);

        m_sbtBuffer->Unmap(0, nullptr);
    }

    D3D12_GPU_VIRTUAL_ADDRESS_RANGE SBTHelper::GetRayGenRange() const
    {
        return {m_sbtBuffer->GetGPUVirtualAddress(), m_rayGenSectionSize};
    }

    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE SBTHelper::GetMissRange() const
    {
        return {m_sbtBuffer->GetGPUVirtualAddress() + m_rayGenSectionSize, m_missSectionSize,
                m_missEntrySize};
    }

    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE SBTHelper::GetHitGroupRange() const
    {
        return {m_sbtBuffer->GetGPUVirtualAddress() + m_rayGenSectionSize + m_missSectionSize,
                m_hitGroupSectionSize, m_hitGroupEntrySize};
    }
}