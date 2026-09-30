#include "stdafx.h"
#include "../public/PSOPrecache.h"

#include "Programs/public/Hash.h"
#include "Programs/public/Log.h"
#include "Runtime/Core/public/DX12PipelineState.h"
#include "Runtime/Core/public/DX12RootSignature.h"

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <queue>
#include <thread>

namespace ElysiaRenderer
{
    using namespace ElysiaCore;

    namespace
    {
        uint64_t HashBytes(const void* data, size_t size)
        {
            if (data == nullptr || size == 0)
                return 0;
            return static_cast<uint64_t>(xxh::GetHash(data, size));
        }

        uint64_t HashBytecode(const D3D12_SHADER_BYTECODE& bytecode)
        {
            return HashBytes(bytecode.pShaderBytecode, bytecode.BytecodeLength);
        }

        uint64_t HashInputLayout(const D3D12_INPUT_LAYOUT_DESC& layout)
        {
            if (layout.pInputElementDescs == nullptr || layout.NumElements == 0)
                return 0;

            std::vector<uint8_t> bytes;
            bytes.reserve(static_cast<size_t>(layout.NumElements) * 48);
            auto append = [&](const void* data, size_t size)
            {
                const auto* begin = static_cast<const uint8_t*>(data);
                bytes.insert(bytes.end(), begin, begin + size);
            };

            for (UINT i = 0; i < layout.NumElements; ++i)
            {
                const D3D12_INPUT_ELEMENT_DESC& element = layout.pInputElementDescs[i];
                const uint32_t semanticIndex = element.SemanticIndex;
                const uint32_t format = static_cast<uint32_t>(element.Format);
                const uint32_t slot = element.InputSlot;
                const uint32_t offset = element.AlignedByteOffset;
                const uint32_t slotClass = static_cast<uint32_t>(element.InputSlotClass);
                const uint32_t stepRate = element.InstanceDataStepRate;
                append(&semanticIndex, sizeof(semanticIndex));
                append(&format, sizeof(format));
                append(&slot, sizeof(slot));
                append(&offset, sizeof(offset));
                append(&slotClass, sizeof(slotClass));
                append(&stepRate, sizeof(stepRate));

                const char* semanticName = element.SemanticName != nullptr ? element.SemanticName : "";
                const uint32_t nameLength = static_cast<uint32_t>(std::strlen(semanticName));
                append(&nameLength, sizeof(nameLength));
                append(semanticName, nameLength);
            }

            return HashBytes(bytes.data(), bytes.size());
        }

        uint64_t HashGraphicsState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
        {
            struct PackedState
            {
                D3D12_BLEND_DESC blend;
                D3D12_RASTERIZER_DESC raster;
                D3D12_DEPTH_STENCIL_DESC depth;
                uint32_t renderTargets[8];
                uint32_t renderTargetCount;
                uint32_t depthFormat;
                uint32_t topology;
                uint32_t sampleCount;
                uint32_t sampleQuality;
                uint32_t sampleMask;
            };

            PackedState packed{};
            packed.blend = desc.BlendState;
            packed.raster = desc.RasterizerState;
            packed.depth = desc.DepthStencilState;
            for (UINT i = 0; i < 8; ++i)
                packed.renderTargets[i] = static_cast<uint32_t>(desc.RTVFormats[i]);
            packed.renderTargetCount = desc.NumRenderTargets;
            packed.depthFormat = static_cast<uint32_t>(desc.DSVFormat);
            packed.topology = static_cast<uint32_t>(desc.PrimitiveTopologyType);
            packed.sampleCount = desc.SampleDesc.Count;
            packed.sampleQuality = desc.SampleDesc.Quality;
            packed.sampleMask = desc.SampleMask;
            return static_cast<uint64_t>(xxh::GetHash(packed));
        }

        struct DiskIdentity
        {
            uint64_t a;
            uint64_t b;
            uint64_t c;
            uint64_t d;
            uint32_t kind;
            uint32_t pad;
        };

        std::filesystem::path CacheFilePath()
        {
            WCHAR assetsPath[512] = {};
            ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));
            return std::filesystem::path(assetsPath) / "Saved" / "PSOCache" / "D3D12.pso";
        }

        constexpr char kCacheMagic[8] = {'E', 'P', 'S', 'O', 'C', 'A', 'C', 'H'};
        constexpr uint32_t kCacheVersion = 1;
        constexpr uint32_t kMaxBlobBytes = 64u * 1024u * 1024u;
    }

    struct PSOPrecache::Impl
    {
        struct GraphicsKey
        {
            uint64_t vs;
            uint64_t ps;
            uint64_t input;
            uint64_t state;
            uint64_t root;

            bool operator==(const GraphicsKey& other) const
            {
                return vs == other.vs && ps == other.ps && input == other.input &&
                       state == other.state && root == other.root;
            }
        };

        struct GraphicsKeyHash
        {
            size_t operator()(const GraphicsKey& key) const
            {
                return static_cast<size_t>(xxh::GetHash(key));
            }
        };

        struct ComputeKey
        {
            uint64_t cs;
            uint64_t root;

            bool operator==(const ComputeKey& other) const
            {
                return cs == other.cs && root == other.root;
            }
        };

        struct ComputeKeyHash
        {
            size_t operator()(const ComputeKey& key) const
            {
                return static_cast<size_t>(xxh::GetHash(key));
            }
        };

        struct Record
        {
            std::unique_ptr<PipelineStateObject> object;
            std::atomic<bool> ready{false};
            std::atomic<bool> failed{false};
            bool isCompute = false;
            D3D12_GRAPHICS_PIPELINE_STATE_DESC graphics{};
            D3D12_COMPUTE_PIPELINE_STATE_DESC compute{};
            std::vector<D3D12_INPUT_ELEMENT_DESC> elements;
            std::vector<std::string> semantics;
            std::vector<uint8_t> diskBlob;
            uint64_t diskKey = 0;
            DX12RootSignature* root = nullptr;
            std::string name;
        };

        ID3D12Device* device = nullptr;
        PSOPrecacheSettings settings{};
        bool async = false;
        bool batchOpen = false;
        bool stop = false;
        int threadCount = 0;

        std::mutex mutex;
        std::condition_variable cv;
        std::queue<std::function<void()>> queue;
        std::vector<std::thread> workers;
        uint64_t submitted = 0;
        uint64_t completed = 0;
        std::exception_ptr error;

        std::unordered_map<GraphicsKey, std::unique_ptr<Record>, GraphicsKeyHash> graphics;
        std::unordered_map<ComputeKey, std::unique_ptr<Record>, ComputeKeyHash> computePSOs;

        std::mutex diskMutex;
        std::unordered_map<uint64_t, std::vector<uint8_t>> diskBlobs;
        std::unordered_set<uint64_t> diskTouched;

        std::atomic<uint32_t> batchCreates{0};
        std::atomic<uint32_t> batchDiskHits{0};
        std::atomic<uint64_t> batchDriverTicks{0};
        std::atomic<uint32_t> lifetimeMisses{0};
        std::atomic<uint32_t> lifetimeTooLate{0};
        LARGE_INTEGER qpcFrequency{};
        LARGE_INTEGER batchWallStart{};

        void ThreadMain();
        void Enqueue(std::function<void()> work);
        void Compile(Record* record, bool inBatch);
        PipelineStateObject* Finish(Record* record, bool created, bool inBatch);
        void WaitReady(Record* record);
        void LoadCache();
        void SaveCache();
        void RememberBlob(uint64_t diskKey, std::vector<uint8_t> blob);
        void ForgetBlob(uint64_t diskKey);
    };

    void PSOPrecache::Impl::ThreadMain()
    {
        SetThreadDescription(GetCurrentThread(), L"PSO Precache");

        for (;;)
        {
            std::function<void()> job;
            {
                std::unique_lock lock(mutex);
                cv.wait(lock, [&]() { return stop || !queue.empty(); });
                if (queue.empty())
                    return;
                job = std::move(queue.front());
                queue.pop();
            }

            std::exception_ptr jobError;
            try
            {
                job();
            }
            catch (...)
            {
                jobError = std::current_exception();
            }

            {
                std::lock_guard lock(mutex);
                if (jobError && !error)
                    error = jobError;
                ++completed;
            }
            cv.notify_all();
        }
    }

    void PSOPrecache::Impl::Enqueue(std::function<void()> work)
    {
        {
            std::lock_guard lock(mutex);
            queue.push(std::move(work));
            ++submitted;
        }
        cv.notify_one();
    }

    void PSOPrecache::Impl::RememberBlob(uint64_t diskKey, std::vector<uint8_t> blob)
    {
        std::lock_guard lock(diskMutex);
        diskBlobs[diskKey] = std::move(blob);
        diskTouched.insert(diskKey);
    }

    void PSOPrecache::Impl::ForgetBlob(uint64_t diskKey)
    {
        std::lock_guard lock(diskMutex);
        diskBlobs.erase(diskKey);
        diskTouched.erase(diskKey);
    }

    void PSOPrecache::Impl::Compile(Record* record, bool inBatch)
    {
        {
            std::lock_guard lock(diskMutex);
            const auto it = diskBlobs.find(record->diskKey);
            if (it != diskBlobs.end())
                record->diskBlob = it->second;
        }

        const bool hadDiskBlob = !record->diskBlob.empty();
        LARGE_INTEGER start{};
        LARGE_INTEGER end{};
        QueryPerformanceCounter(&start);

        ComPtr<ID3D12PipelineState> pipeline;
        HRESULT hr = E_FAIL;
        bool diskHit = false;
        if (record->isCompute)
        {
            D3D12_COMPUTE_PIPELINE_STATE_DESC desc = record->compute;
            if (hadDiskBlob)
            {
                desc.CachedPSO.pCachedBlob = record->diskBlob.data();
                desc.CachedPSO.CachedBlobSizeInBytes = record->diskBlob.size();
            }
            hr = device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pipeline));
            if (SUCCEEDED(hr) && hadDiskBlob)
            {
                diskHit = true;
            }
            else if (FAILED(hr) && hadDiskBlob)
            {
                ForgetBlob(record->diskKey);
                record->diskBlob.clear();
                desc.CachedPSO = {};
                hr = device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pipeline));
            }
        }
        else
        {
            for (size_t i = 0; i < record->elements.size(); ++i)
                record->elements[i].SemanticName = record->semantics[i].c_str();

            D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = record->graphics;
            desc.InputLayout.pInputElementDescs = record->elements.empty()
                                                      ? nullptr
                                                      : record->elements.data();
            desc.InputLayout.NumElements = static_cast<UINT>(record->elements.size());
            if (hadDiskBlob)
            {
                desc.CachedPSO.pCachedBlob = record->diskBlob.data();
                desc.CachedPSO.CachedBlobSizeInBytes = record->diskBlob.size();
            }
            hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline));
            if (SUCCEEDED(hr) && hadDiskBlob)
            {
                diskHit = true;
            }
            else if (FAILED(hr) && hadDiskBlob)
            {
                ForgetBlob(record->diskKey);
                record->diskBlob.clear();
                desc.CachedPSO = {};
                hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline));
            }
        }

        QueryPerformanceCounter(&end);
        ElysiaHelper::ThrowIfFailed(hr);

        if (inBatch)
        {
            batchCreates.fetch_add(1, std::memory_order_relaxed);
            batchDriverTicks.fetch_add(static_cast<uint64_t>(end.QuadPart - start.QuadPart),
                                       std::memory_order_relaxed);
            if (diskHit)
                batchDiskHits.fetch_add(1, std::memory_order_relaxed);
        }

        if (!record->name.empty())
        {
            const std::wstring wide(record->name.begin(), record->name.end());
            pipeline->SetName(wide.c_str());
        }

        ComPtr<ID3DBlob> cached;
        if (SUCCEEDED(pipeline->GetCachedBlob(&cached)) && cached != nullptr &&
            cached->GetBufferSize() > 0 && cached->GetBufferSize() <= kMaxBlobBytes)
        {
            std::vector<uint8_t> bytes(cached->GetBufferSize());
            std::memcpy(bytes.data(), cached->GetBufferPointer(), bytes.size());
            RememberBlob(record->diskKey, std::move(bytes));
        }

        if (record->isCompute)
        {
            record->object->m_pipelineState = std::make_unique<DX12ComputePipelineState>(
                pipeline,
                record->root);
        }
        else
        {
            record->object->m_pipelineState = std::make_unique<DX12GraphicsPipelineState>(
                pipeline,
                record->root);
        }
        record->ready.store(true, std::memory_order_release);
    }

    void PSOPrecache::Impl::WaitReady(Record* record)
    {
        if (record->ready.load(std::memory_order_acquire))
            return;

        std::unique_lock lock(mutex);
        cv.wait(lock, [&]()
        {
            return record->ready.load(std::memory_order_acquire) ||
                   record->failed.load(std::memory_order_acquire) ||
                   stop;
        });
    }

    PipelineStateObject* PSOPrecache::Impl::Finish(Record* record, bool created, bool inBatch)
    {
        if (!created)
        {
            if (!record->ready.load(std::memory_order_acquire) && !inBatch)
            {
                if (!record->failed.load(std::memory_order_acquire))
                {
                    lifetimeTooLate.fetch_add(1, std::memory_order_relaxed);
                    if (settings.validation >= 1)
                    {
                        ElysiaHelper::Log::Warn(
                            "PSO precache too late: %s '%s' was still compiling when it was requested.",
                            record->isCompute ? "Compute" : "Graphics",
                            record->name.c_str());
                    }
                }
                WaitReady(record);
            }
            return record->object.get();
        }

        if (!inBatch && settings.validation >= 1)
        {
            lifetimeMisses.fetch_add(1, std::memory_order_relaxed);
            ElysiaHelper::Log::Warn(
                "PSO precache miss: %s '%s' compiled on the calling thread.",
                record->isCompute ? "Compute" : "Graphics",
                record->name.c_str());
            if (settings.validation >= 2)
            {
                ElysiaHelper::Log::Warn("PSO precache miss detail: diskKey=%016llX",
                                        static_cast<unsigned long long>(record->diskKey));
            }
        }

        auto compile = [this, record, inBatch]()
        {
            try
            {
                Compile(record, inBatch);
            }
            catch (...)
            {
                record->failed.store(true, std::memory_order_release);
                throw;
            }
        };

        if (inBatch && async)
            Enqueue(compile);
        else
            compile();

        return record->object.get();
    }

    void PSOPrecache::Impl::LoadCache()
    {
        const std::filesystem::path path = CacheFilePath();
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return;

        char magic[8] = {};
        uint32_t version = 0;
        uint32_t count = 0;
        file.read(magic, sizeof(magic));
        file.read(reinterpret_cast<char*>(&version), sizeof(version));
        file.read(reinterpret_cast<char*>(&count), sizeof(count));
        if (!file || std::memcmp(magic, kCacheMagic, sizeof(magic)) != 0 || version != kCacheVersion)
        {
            ElysiaHelper::Log::Warn("PSO precache: ignored unreadable cache %s",
                                    path.string().c_str());
            return;
        }

        std::lock_guard lock(diskMutex);
        for (uint32_t i = 0; i < count; ++i)
        {
            uint64_t diskKey = 0;
            uint32_t blobSize = 0;
            file.read(reinterpret_cast<char*>(&diskKey), sizeof(diskKey));
            file.read(reinterpret_cast<char*>(&blobSize), sizeof(blobSize));
            if (!file || blobSize == 0 || blobSize > kMaxBlobBytes)
            {
                diskBlobs.clear();
                ElysiaHelper::Log::Warn("PSO precache: ignored truncated cache %s",
                                        path.string().c_str());
                return;
            }

            std::vector<uint8_t> blob(blobSize);
            file.read(reinterpret_cast<char*>(blob.data()), blobSize);
            if (!file)
            {
                diskBlobs.clear();
                ElysiaHelper::Log::Warn("PSO precache: ignored truncated cache %s",
                                        path.string().c_str());
                return;
            }
            diskBlobs.emplace(diskKey, std::move(blob));
        }
    }

    void PSOPrecache::Impl::SaveCache()
    {
        std::vector<std::pair<uint64_t, std::vector<uint8_t>>> entries;
        {
            std::lock_guard lock(diskMutex);
            entries.reserve(diskTouched.size());
            for (uint64_t diskKey : diskTouched)
            {
                const auto it = diskBlobs.find(diskKey);
                if (it == diskBlobs.end() || it->second.empty())
                    continue;
                entries.emplace_back(diskKey, it->second);
            }
        }
        if (entries.empty())
            return;

        const std::filesystem::path path = CacheFilePath();
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);

        const std::filesystem::path temporary = path.string() + ".tmp";
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            if (!file)
            {
                ElysiaHelper::Log::Warn("PSO precache: failed to write %s",
                                        temporary.string().c_str());
                return;
            }

            const uint32_t version = kCacheVersion;
            const uint32_t count = static_cast<uint32_t>(entries.size());
            file.write(kCacheMagic, sizeof(kCacheMagic));
            file.write(reinterpret_cast<const char*>(&version), sizeof(version));
            file.write(reinterpret_cast<const char*>(&count), sizeof(count));
            for (const auto& entry : entries)
            {
                const uint32_t blobSize = static_cast<uint32_t>(entry.second.size());
                file.write(reinterpret_cast<const char*>(&entry.first), sizeof(entry.first));
                file.write(reinterpret_cast<const char*>(&blobSize), sizeof(blobSize));
                file.write(reinterpret_cast<const char*>(entry.second.data()), blobSize);
            }
            if (!file)
            {
                ElysiaHelper::Log::Warn("PSO precache: failed to write %s",
                                        temporary.string().c_str());
                return;
            }
        }

        std::filesystem::remove(path, error);
        std::filesystem::rename(temporary, path, error);
        if (error)
        {
            ElysiaHelper::Log::Warn("PSO precache: failed to replace %s", path.string().c_str());
        }
    }

    PSOPrecache& PSOPrecache::Get()
    {
        static PSOPrecache instance;
        return instance;
    }

    PSOPrecache::~PSOPrecache()
    {
        Shutdown();
    }

    void PSOPrecache::Startup(ID3D12Device* device, const PSOPrecacheSettings& settings)
    {
        if (m_impl != nullptr)
            return;

        m_impl = std::make_unique<Impl>();
        m_impl->device = device;
        m_impl->settings = settings;
        QueryPerformanceFrequency(&m_impl->qpcFrequency);
        m_impl->LoadCache();

        if (!settings.enable)
        {
            m_impl->threadCount = 1;
            return;
        }

        unsigned hardwareThreads = std::thread::hardware_concurrency();
        if (hardwareThreads == 0)
            hardwareThreads = 4;

        const int percent = std::clamp(settings.threadPercent, 1, 100);
        int count = static_cast<int>(hardwareThreads) * percent / 100;
        if (count < 1)
            count = 1;

        const int threadMin = std::max(settings.threadMin, 1);
        const int threadMax = std::max(settings.threadMax, threadMin);
        count = std::clamp(count, threadMin, threadMax);

        m_impl->async = count > 0;
        m_impl->threadCount = count;
        m_impl->workers.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i)
            m_impl->workers.emplace_back([impl = m_impl.get()]() { impl->ThreadMain(); });
    }

    void PSOPrecache::Shutdown()
    {
        if (m_impl == nullptr)
            return;

        {
            std::lock_guard lock(m_impl->mutex);
            m_impl->stop = true;
        }
        m_impl->cv.notify_all();
        for (std::thread& worker : m_impl->workers)
        {
            if (worker.joinable())
                worker.join();
        }

        m_impl->SaveCache();

        const uint32_t misses = m_impl->lifetimeMisses.load(std::memory_order_relaxed);
        const uint32_t tooLate = m_impl->lifetimeTooLate.load(std::memory_order_relaxed);
        if (m_impl->settings.validation >= 1 && (misses > 0 || tooLate > 0))
        {
            ElysiaHelper::Log::Info("PSO precache shutdown: misses %u, too late %u",
                                    misses,
                                    tooLate);
        }

        m_impl->graphics.clear();
        m_impl->computePSOs.clear();
        m_impl.reset();
    }

    void PSOPrecache::BeginBatch()
    {
        if (m_impl == nullptr)
            return;

        m_impl->batchCreates.store(0, std::memory_order_relaxed);
        m_impl->batchDiskHits.store(0, std::memory_order_relaxed);
        m_impl->batchDriverTicks.store(0, std::memory_order_relaxed);
        QueryPerformanceCounter(&m_impl->batchWallStart);
        m_impl->batchOpen = true;
    }

    void PSOPrecache::WaitBatch()
    {
        if (m_impl == nullptr)
            return;

        m_impl->batchOpen = false;

        std::exception_ptr error;
        {
            std::unique_lock lock(m_impl->mutex);
            m_impl->cv.wait(lock, [&]() { return m_impl->completed >= m_impl->submitted; });
            error = m_impl->error;
            m_impl->error = nullptr;
        }

        const uint32_t creates = m_impl->batchCreates.load(std::memory_order_relaxed);
        const uint32_t diskHits = m_impl->batchDiskHits.load(std::memory_order_relaxed);
        if (m_impl->settings.validation >= 1 && creates > 0)
        {
            LARGE_INTEGER wallEnd{};
            QueryPerformanceCounter(&wallEnd);
            const double wallMs = m_impl->qpcFrequency.QuadPart == 0
                                      ? 0.0
                                      : static_cast<double>(wallEnd.QuadPart -
                                                            m_impl->batchWallStart.QuadPart) *
                                        1000.0 / static_cast<double>(m_impl->qpcFrequency.QuadPart);
            const double driverMs = m_impl->qpcFrequency.QuadPart == 0
                                        ? 0.0
                                        : static_cast<double>(m_impl->batchDriverTicks.load(
                                              std::memory_order_relaxed)) *
                                          1000.0 /
                                          static_cast<double>(m_impl->qpcFrequency.QuadPart);
            const uint32_t diskMisses = creates > diskHits ? creates - diskHits : 0;
            ElysiaHelper::Log::Info(
                "PSO precache: %u pipelines, wall %.0f ms, driver %.0f ms on %d threads, disk hits %u, disk misses %u",
                creates,
                wallMs,
                driverMs,
                m_impl->threadCount,
                diskHits,
                diskMisses);
        }

        m_impl->SaveCache();
        if (error)
            std::rethrow_exception(error);
    }

    PipelineStateObject* PSOPrecache::GetGraphics(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
                                                  DX12RootSignature* rootSignature,
                                                  const std::string& name)
    {
        if (m_impl == nullptr)
            return nullptr;

        Impl::GraphicsKey key{};
        key.vs = HashBytecode(desc.VS);
        key.ps = HashBytecode(desc.PS);
        key.input = HashInputLayout(desc.InputLayout);
        key.state = HashGraphicsState(desc);
        key.root = reinterpret_cast<uint64_t>(desc.pRootSignature);

        DiskIdentity identity{};
        identity.a = key.vs;
        identity.b = key.ps;
        identity.c = key.input;
        identity.d = key.state;
        identity.kind = 1;

        const bool inBatch = m_impl->batchOpen;
        Impl::Record* record = nullptr;
        bool created = false;
        {
            std::lock_guard lock(m_impl->mutex);
            const auto it = m_impl->graphics.find(key);
            if (it != m_impl->graphics.end())
            {
                record = it->second.get();
            }
            else
            {
                auto owned = std::make_unique<Impl::Record>();
                record = owned.get();
                record->isCompute = false;
                record->graphics = desc;
                record->graphics.CachedPSO = {};
                record->graphics.InputLayout = {};
                record->root = rootSignature;
                record->name = name;
                record->diskKey = static_cast<uint64_t>(xxh::GetHash(identity));
                record->object = std::make_unique<PipelineStateObject>();
                record->object->m_pipelineType = PipelineType::Graphics;
                record->object->m_rootSignature = rootSignature;

                if (desc.InputLayout.pInputElementDescs != nullptr && desc.InputLayout.NumElements > 0)
                {
                    record->elements.assign(
                        desc.InputLayout.pInputElementDescs,
                        desc.InputLayout.pInputElementDescs + desc.InputLayout.NumElements);
                    record->semantics.resize(record->elements.size());
                    for (size_t i = 0; i < record->elements.size(); ++i)
                    {
                        const char* semanticName = record->elements[i].SemanticName;
                        record->semantics[i] = semanticName != nullptr ? semanticName : "";
                        record->elements[i].SemanticName = nullptr;
                    }
                }

                m_impl->graphics.emplace(key, std::move(owned));
                created = true;
            }
        }

        return m_impl->Finish(record, created, inBatch);
    }

    PipelineStateObject* PSOPrecache::GetCompute(const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc,
                                                 DX12RootSignature* rootSignature,
                                                 const std::string& name)
    {
        if (m_impl == nullptr)
            return nullptr;

        Impl::ComputeKey key{};
        key.cs = HashBytecode(desc.CS);
        key.root = reinterpret_cast<uint64_t>(desc.pRootSignature);

        DiskIdentity identity{};
        identity.a = key.cs;
        identity.kind = 2;

        const bool inBatch = m_impl->batchOpen;
        Impl::Record* record = nullptr;
        bool created = false;
        {
            std::lock_guard lock(m_impl->mutex);
            const auto it = m_impl->computePSOs.find(key);
            if (it != m_impl->computePSOs.end())
            {
                record = it->second.get();
            }
            else
            {
                auto owned = std::make_unique<Impl::Record>();
                record = owned.get();
                record->isCompute = true;
                record->compute = desc;
                record->compute.CachedPSO = {};
                record->root = rootSignature;
                record->name = name;
                record->diskKey = static_cast<uint64_t>(xxh::GetHash(identity));
                record->object = std::make_unique<PipelineStateObject>();
                record->object->m_pipelineType = PipelineType::Compute;
                record->object->m_rootSignature = rootSignature;
                m_impl->computePSOs.emplace(key, std::move(owned));
                created = true;
            }
        }

        return m_impl->Finish(record, created, inBatch);
    }

    void PSOPrecache::Enqueue(std::function<void()> work, const char* name)
    {
        if (m_impl == nullptr)
        {
            work();
            return;
        }

        if (!m_impl->batchOpen || !m_impl->async)
        {
            work();
            return;
        }

        const std::string label = name != nullptr ? name : "PSO work";
        m_impl->Enqueue([job = std::move(work), label, validation = m_impl->settings.validation]()
        {
            try
            {
                job();
            }
            catch (...)
            {
                if (validation >= 1)
                {
                    ElysiaHelper::Log::Error("PSO precache: '%s' failed on a worker thread.",
                                             label.c_str());
                }
                throw;
            }
        });
    }
}
