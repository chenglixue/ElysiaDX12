#pragma once
#include "Benchmark.h"

namespace ElysiaCore
{
    class DX12Device;
}

namespace ElysiaHelper
{
    class GPUTimestamps
    {
    public:
        void OnCreate(ElysiaCore::DX12Device *pDevice, uint32_t numberOfBackBuffers);
        void OnDestroy();

        void GetTimeStamp(ID3D12GraphicsCommandList *pCommandList, const char *label);
        void GetTimeStampUser(const TimeStamp &ts);
        void CollectTimings(ID3D12GraphicsCommandList *pCommandList);

        void OnBeginFrame(UINT64 gpuTicksPerSecond, std::vector<TimeStamp> *pTimestamps);
        void OnEndFrame();

    private:
        const uint32_t MaxValuesPerFrame = 128;

        ID3D12Resource    *m_pBuffer = NULL;
        ID3D12QueryHeap   *m_pQueryHeap = NULL;

        uint32_t m_frame = 0;
        uint32_t m_NumberOfBackBuffers = 0;

        std::vector<std::string> m_labels[5];
        std::vector<TimeStamp> m_cpuTimeStamps[5];
    };
}

