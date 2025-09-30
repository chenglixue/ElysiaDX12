#pragma once
#include "stdafx.h"
#include "Mesh.h"
#include "Material.h"
#include "DX12MeshRender.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "BoundingBox.h"
#include "DX12Device.h"
#include "BufferManager.h"
#include "DX12MeshRender.h"
#include "TextureManager.h"


namespace ElysiaModel
{
	using namespace ElysiaRenderer;
	using namespace ElysiaHelper;

	class ModelImporter
	{
	public:
		ModelImporter() = default;
		ModelImporter(DX12Device* pDevice, BufferManager* pBufferManager, TextureManager* pTextureManager);
		ModelImporter(const ModelImporter& rhs) = delete;
		ModelImporter& operator=(const ModelImporter& rhs) = delete;
		ModelImporter(ModelImporter&& rhs) = default;
		~ModelImporter();

		UINT GetMeshCount() const noexcept
		{
			return m_meshData.meshCount;
		}
		const Mesh& GetMesh(UINT meshIndex) const 
		{
			assert(meshIndex < m_meshData.meshCount);
			return m_pMesh[meshIndex];
		}

		UINT GetMaterialCount() const noexcept
		{
			return m_meshData.materialCount;
		}
		const Material& GetMaterial(UINT materialIndex) const
		{
			assert(materialIndex < m_meshData.materialCount);
			return m_pMaterial[materialIndex];
		}

		UINT GetVertexStride() const noexcept
		{
			return m_vertexStride;
		}

		const AxisAlignedBox& GetBoundingBox() const noexcept
		{
			return m_meshData.boundingBox;
		}

		const MeshRender& GetMeshRenderer(UINT meshRendererIndex) const
		{
			assert(meshRendererIndex < m_meshData.meshCount);

			return m_pMeshRender[meshRendererIndex];
		}

		bool Load(const LPCWSTR& fileName);
		bool Load(const std::vector<LPCWSTR>& fileNames);

		void ComputeMeshBoundingBox(uint32_t meshIndex, AxisAlignedBox& bbox) const;
		void ComputeGlobalBoundingBox(AxisAlignedBox& bbox) const;
		void ComputeAllBoundingBoxes();

		void LoadTextures(const std::wstring& filePath);

		bool CreateVertexBuffer();
		bool CreateIndexBuffer();

		void CreateMeshRenders();

		void PrintModelStats();

	private:
		DX12Device* m_pDevice = nullptr;
		BufferManager* m_pBufferManager = nullptr;
		TextureManager* m_pTextureManager = nullptr;

		MeshData	m_meshData{};
		Material*	m_pMaterial = nullptr;
		Mesh*		m_pMesh = nullptr;
		MeshRender* m_pMeshRender = nullptr;
		
		UINT m_vertexStride = 0;

		uint8_t* m_pVertexData;
		uint8_t* m_pIndexData;
	};


}