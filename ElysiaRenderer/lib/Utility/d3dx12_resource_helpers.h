#pragma once
#include <d3d12.h>
#include <cstdint>

// ================================
// 简化版 d3dx12.h：仅包含 UpdateSubresources 所需定义
// ================================

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

}}} // namespace Microsoft::Direct3D::Helpers

// 使用宏简化调用
#define UpdateSubresources(commandList, dest, interm, offset, firstSub, numSub, layouts) \
    Microsoft::Direct3D::Helpers::UpdateSubresources(commandList, dest, interm, offset, firstSub, numSub, layouts)