#include "stdafx.h"
#include "GPUTimestamps.h"

#include "Programs/Helper.h"
#include "Runtime/Core/DX12Device.h"

namespace ElysiaHelper
{
    void GPUTimestamps::OnCreate(ElysiaCore::DX12Device *pDevice, uint32_t numberOfBackBuffers)
    {
        m_NumberOfBackBuffers = numberOfBackBuffers;

        D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
        queryHeapDesc.Count = MaxValuesPerFrame * numberOfBackBuffers;
        queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        queryHeapDesc.NodeMask = 0;
        ThrowIfFailed(pDevice->GetDevice()->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&m_pQueryHeap)));

        auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint64_t) * numberOfBackBuffers * MaxValuesPerFrame);
        ThrowIfFailed(
            pDevice->GetDevice()->CreateCommittedResource(
                &heapProperties,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&m_pBuffer))
        );
        SetName(m_pBuffer, "GPUTimestamps::m_pBuffer");
    }

    void GPUTimestamps::OnDestroy()
    {
        m_pBuffer->Release();

        m_pQueryHeap->Release();
    }

    void GPUTimestamps::GetTimeStamp(ID3D12GraphicsCommandList *pCommandList, const char *label)
    {
        uint32_t numMeasurements = (uint32_t)m_labels[m_frame].size();
        pCommandList->EndQuery(m_pQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, m_frame*MaxValuesPerFrame + numMeasurements);
        m_labels[m_frame].push_back(label);
    }

    void GPUTimestamps::GetTimeStampUser(const TimeStamp& ts)
    {
        m_cpuTimeStamps[m_frame].push_back(ts);
    }

    void GPUTimestamps::CollectTimings(ID3D12GraphicsCommandList *pCommandList)
    {
        uint32_t numMeasurements = (uint32_t)m_labels[m_frame].size();

        pCommandList->ResolveQueryData(m_pQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, m_frame*MaxValuesPerFrame, numMeasurements, m_pBuffer, m_frame * MaxValuesPerFrame* sizeof(UINT64));
    }

    void GPUTimestamps::OnBeginFrame(UINT64 gpuTicksPerSecond, std::vector<TimeStamp> *pTimestamps)
    {
        std::vector<TimeStamp> &cpuTimeStamps = m_cpuTimeStamps[m_frame];
        std::vector<std::string> &gpuLabels = m_labels[m_frame];

        pTimestamps->clear();
        pTimestamps->reserve(cpuTimeStamps.size() + gpuLabels.size());

        // copy CPU timestamps
        //
        for (uint32_t i = 0; i < cpuTimeStamps.size(); i++)
        {
            pTimestamps->push_back(cpuTimeStamps[i]);
        }

        // copy GPU timestamps
        //
        uint32_t numMeasurements = (uint32_t)gpuLabels.size();
        if (numMeasurements > 0)
        {
            double microsecondsPerTick = 1000000.0 / (double)gpuTicksPerSecond;

            uint32_t ini = MaxValuesPerFrame * m_frame;
            uint32_t fin = MaxValuesPerFrame * (m_frame + 1);

            CD3DX12_RANGE readRange(ini * sizeof(UINT64), fin * sizeof(UINT64));
            UINT64 *pTimingsBuffer = NULL;
            ThrowIfFailed(m_pBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pTimingsBuffer)));

            UINT64 *pTimingsInTicks = &pTimingsBuffer[ini];

            for (uint32_t i = 1; i < numMeasurements; i++)
            {
                TimeStamp ts = { gpuLabels[i], float(microsecondsPerTick * (double)(pTimingsInTicks[i] - pTimingsInTicks[i-1])) };
                pTimestamps->push_back(ts);
            }

            // compute total
            TimeStamp ts = { "Total GPU Time", float(microsecondsPerTick * (double)(pTimingsInTicks[numMeasurements - 1] - pTimingsInTicks[0])) };
            pTimestamps->push_back(ts);

            CD3DX12_RANGE writtenRange(0, 0);
            m_pBuffer->Unmap(0, &writtenRange);
        }

        // we always need to clear these ones
        cpuTimeStamps.clear();
        gpuLabels.clear();
    }

    void GPUTimestamps::OnEndFrame()
    {
        m_frame = (m_frame + 1) % m_NumberOfBackBuffers;
    }
}
