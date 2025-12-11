#pragma once
#include <d3d12.h>
#include <cstdint>

namespace Microsoft {
namespace Direct3D {
namespace Helpers {

inline UINT64 UpdateSubresources(
    _In_ ID3D12GraphicsCommandList* commandList,
    _In_ ID3D12Resource* destinationResource,
    _In_ ID3D12Resource* intermediateResource,
    UINT64 intermediateOffset,
    UINT firstSubresource,
    UINT numSubresources,
    _In_reads_opt_(numSubresources) D3D12_PLACED_SUBRESOURCE_FOOTPRINT* layouts)
{
    if (!layouts && numSubresources > 1)
    {
        // 如果没提供 layout，需要先查询
        auto device = static_cast<ID3D12Device*>(nullptr);
        D3D12_RESOURCE_DESC const desc = destinationResource->GetDesc();
        commandList->GetDevice(IID_PPV_ARGS(&device));
        UINT64 size = 0;
        std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> localLayouts(numSubresources);
        std::vector<UINT> numRows(numSubresources);
        std::vector<UINT64> rowSizesInBytes(numSubresources);
        device->GetCopyableFootprints(&desc, firstSubresource, numSubresources, intermediateOffset, localLayouts.data(), numRows.data(), rowSizesInBytes.data(), &size);
        layouts = localLayouts.data();
        device->Release();

        // 复制数据
        for (UINT i = 0; i < numSubresources; ++i)
        {
            D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
            srcLocation.pResource = intermediateResource;
            srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            srcLocation.PlacedFootprint = layouts[i];

            D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
            dstLocation.pResource = destinationResource;
            dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dstLocation.SubresourceIndex = i + firstSubresource;

            commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);
        }

        return size;
    }
    else if (!layouts && numSubresources == 1)
    {
        // 单个子资源且无 layout —— 不支持
        return 0;
    }

    // 已提供 layouts，直接使用
    for (UINT i = 0; i < numSubresources; ++i)
    {
        D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
        srcLocation.pResource = intermediateResource;
        srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLocation.PlacedFootprint = layouts[i];

        D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
        dstLocation.pResource = destinationResource;
        dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLocation.SubresourceIndex = i + firstSubresource;

        commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);
    }

    // 计算总大小
    UINT64 totalSize = 0;
    D3D12_RESOURCE_DESC const destDesc = destinationResource->GetDesc();
    for (UINT i = 0; i < numSubresources; ++i)
    {
        UINT64 footprintMemSize = 0;
        UINT numRows = 0;
        UINT64 rowSizeInBytes = 0;
        ID3D12Device* device = nullptr;
        destinationResource->GetDevice(IID_PPV_ARGS(&device));

        device->GetCopyableFootprints(&destDesc, i + firstSubresource, 1, layouts[i].Offset, &layouts[i], &numRows, &rowSizeInBytes, &footprintMemSize);
        totalSize += footprintMemSize;
        device->Release();
    }

    return totalSize;
}

    inline UINT64 UpdateSubresources(
    _In_ ID3D12GraphicsCommandList* pCmdList,
    _In_ ID3D12Resource* pDestinationResource,
    _In_ ID3D12Resource* pIntermediate,
    UINT64 IntermediateOffset,
    _In_range_(0,D3D12_REQ_SUBRESOURCES) UINT FirstSubresource,
    _In_range_(0,D3D12_REQ_SUBRESOURCES-FirstSubresource) UINT NumSubresources,
    _In_reads_(NumSubresources) const D3D12_SUBRESOURCE_DATA* pSrcData) noexcept
{
    UINT64 RequiredSize = 0;
    const auto MemToAlloc = static_cast<UINT64>(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) + sizeof(UINT) + sizeof(UINT64)) * NumSubresources;
    if (MemToAlloc > SIZE_MAX)
    {
        return 0;
    }
    void* pMem = HeapAlloc(GetProcessHeap(), 0, static_cast<SIZE_T>(MemToAlloc));
    if (pMem == nullptr)
    {
        return 0;
    }
    auto pLayouts = static_cast<D3D12_PLACED_SUBRESOURCE_FOOTPRINT*>(pMem);
    auto pRowSizesInBytes = reinterpret_cast<UINT64*>(pLayouts + NumSubresources);
    auto pNumRows = reinterpret_cast<UINT*>(pRowSizesInBytes + NumSubresources);

#if defined(_MSC_VER) || !defined(_WIN32)
    const auto Desc = pDestinationResource->GetDesc();
#else
    D3D12_RESOURCE_DESC tmpDesc;
    const auto& Desc = *pDestinationResource->GetDesc(&tmpDesc);
#endif
    ID3D12Device* pDevice = nullptr;
    pDestinationResource->GetDevice(IID_ID3D12Device, reinterpret_cast<void**>(&pDevice));
    pDevice->GetCopyableFootprints(&Desc, FirstSubresource, NumSubresources, IntermediateOffset, pLayouts, pNumRows, pRowSizesInBytes, &RequiredSize);
    pDevice->Release();

    const UINT64 Result = UpdateSubresources(pCmdList, pDestinationResource, pIntermediate, FirstSubresource, NumSubresources, RequiredSize, pLayouts, pNumRows, pRowSizesInBytes, pSrcData);
    HeapFree(GetProcessHeap(), 0, pMem);
    return Result;
}

}}} // namespace Microsoft::Direct3D::Helpers

// 使用宏简化调用
#define UpdateSubresources(commandList, dest, interm, offset, firstSub, numSub, layouts) \
    Microsoft::Direct3D::Helpers::UpdateSubresources(commandList, dest, interm, offset, firstSub, numSub, layouts)