#pragma once

#include "Programs/public/Helper.h"

#include <functional>
#include <string>

namespace ElysiaCore
{
    class DX12RootSignature;
    struct PipelineStateObject;
}

namespace ElysiaRenderer
{
    // Background compilation of graphics, compute, and caller-supplied work
    // (ray tracing state objects). A batch opened around pass setup overlaps
    // driver compilation with the next shader compile. Cached blobs are
    // replayed from Saved/PSOCache/D3D12.pso on the next launch.
    struct PSOPrecacheSettings
    {
        bool enable = true;
        int threadPercent = 75;
        int threadMin = 2;
        int threadMax = 6;
        int validation = 1;
    };

    class PSOPrecache
    {
    public:
        static PSOPrecache& Get();
        ~PSOPrecache();

        void Startup(ID3D12Device* device, const PSOPrecacheSettings& settings);
        void Shutdown();

        void BeginBatch();
        void WaitBatch();

        ElysiaCore::PipelineStateObject* GetGraphics(
            const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
            ElysiaCore::DX12RootSignature* rootSignature,
            const std::string& name);
        ElysiaCore::PipelineStateObject* GetCompute(
            const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc,
            ElysiaCore::DX12RootSignature* rootSignature,
            const std::string& name);

        // Runs inline when no batch is open, otherwise on the precache pool.
        void Enqueue(std::function<void()> work, const char* name);

    private:
        PSOPrecache() = default;
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
