#pragma once
#include "Runtime/Engine/ECS/public/Entity.h"
#include "Runtime/Resource/Model/public/LoadedModel.h"
#include "Runtime/Resource/Model/public/SharedTypes.h"

namespace ElysiaRenderer
{
    struct RenderItem
    {
        D3D12_VERTEX_BUFFER_VIEW vbView;
        D3D12_INDEX_BUFFER_VIEW ibView;
        UINT indexCount;
        UINT startIndex;
        UINT baseVertex;

        ElysiaEngine::Entity* pAssociatedEntity = nullptr;

        Matrix worldMatrix;

        MaterialTextureIndices textureIndices;
        ElysiaModel::LoadedMaterial loadedMaterial;

        const ElysiaModel::LoadedMaterial& GetMaterial() const;

        ID3D12PipelineState* pso;

        mutable UINT NumFramesDirty = 3;

        float distanceToCameraSq = 0.f;
    };
}