#pragma once
#include "Runtime/Resource/Model/LoadedModel.h"
#include "Runtime/Resource/Model/SharedTypes.h"

namespace ElysiaRenderer
{
	struct RenderItem
	{
		// 几何数据
		D3D12_VERTEX_BUFFER_VIEW vbView;
		D3D12_INDEX_BUFFER_VIEW  ibView;
		UINT indexCount;
		UINT startIndex;
		INT  baseVertex;
		
		// 变换数据
		DirectX::XMMATRIX worldMatrix;

		MaterialTextureIndices textureIndices;
		ElysiaModel::LoadedMaterial loadedMaterial;

		// 排序依据
		ID3D12PipelineState* pso;
	};
}

