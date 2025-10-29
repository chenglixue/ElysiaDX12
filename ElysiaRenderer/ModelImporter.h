#pragma once
#include "Mesh.h"
#include "Material.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "BoundingBox.h"
#include "DX12Device.h"
#include "BufferManager.h"

#include "DX12MeshRender.h"


namespace ElysiaModel
{
	using namespace ElysiaRenderer;
	using namespace ElysiaHelper;

	class TextureManager;

	class ModelImporter
	{
	public:
		ModelImporter() = default;
		ModelImporter(BufferManager* pBufferManager, TextureManager* pTextureManager);
		ModelImporter(const ModelImporter& rhs) = delete;
		ModelImporter& operator=(const ModelImporter& rhs) = delete;
		ModelImporter(ModelImporter&& rhs) = default;
		~ModelImporter();

		UINT GetMeshCount() const;
		const Mesh& GetMesh(UINT meshIndex) const;

		UINT GetMaterialCount() const;
		const Material& GetMaterial(UINT materialIndex) const;

		UINT GetVertexStride() const;

		const AxisAlignedBox& GetBoundingBox() const;

		const MeshRender& GetMeshRenderer(UINT meshRendererIndex) const;

		bool Load(const LPCWSTR& fileName);
		bool Load(const std::vector<LPCWSTR>& fileNames);
		bool LoadAssimp(const std::string& fileName);
		bool LoadSerialize(const std::string& fileName);
		bool Save(const std::string& fileName);

		void ComputeMeshBoundingBox(uint32_t meshIndex, AxisAlignedBox& bbox) const;
		void ComputeGlobalBoundingBox(AxisAlignedBox& bbox) const;
		void ComputeAllBoundingBoxes();

		void LoadTextures(const std::wstring& filePath);

		bool CreateVertexBuffer();
		bool CreateIndexBuffer();

		void CreateMeshRenders();

		void PrintModelStats();

		void Optimize();
		void OptimizeRemoveDuplicateVertices();
		void OptimizePreTransform();

	private:
		BufferManager* m_pBufferManager = nullptr;
		TextureManager* m_pTextureManager = nullptr;

		MeshData	m_meshData{};
		Material*	m_pMaterial = nullptr;
		Mesh*		m_pMesh = nullptr;
		MeshRender* m_pMeshRender = nullptr;
		
		UINT m_vertexStride = 0;
		UINT m_vertexCount = 0;
		UINT m_indexCount = 0;

		uint8_t* m_pVertexData = nullptr;
		uint8_t* m_pIndexData = nullptr;
	};

	extern std::unique_ptr<ModelImporter> g_pModelImporter;

	inline ModelImporter* GetModelImporter()
	{
		if (g_pModelImporter == nullptr)
		{
			ThrowRuntimeError("null model importer");
		}
		return g_pModelImporter.get();
	}
}