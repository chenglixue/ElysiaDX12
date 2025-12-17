#pragma once
#include "LoadedModel.h"
#include "MaterialData.h"
#include "../DX12/DX12MeshRender.h"
#include "BoundingBox.h"
#include "src/Manager/TextureManager.h"
#include "src/Manager/BufferManager.h"

namespace ElysiaModel
{
	using namespace ElysiaRenderer;
	using namespace ElysiaHelper;

	class ModelImporter
	{
	public:
		ModelImporter() = default;
		ModelImporter(DX12Device* pDevice);
		ModelImporter(const ModelImporter& rhs) = delete;
		ModelImporter& operator=(const ModelImporter& rhs) = delete;
		ModelImporter(ModelImporter&& rhs) = default;
		~ModelImporter();

		bool LoadModel(const char* filePath, bool mergeByMaterial, bool invertTexcoordY, bool importMeshes,
			bool importSkeletons, bool importAnimations, float scale, LoadedModel &model);

		UINT GetMeshCount() const noexcept;
		const LoadedModel& GetMesh(UINT meshIndex) const;

		UINT GetMaterialCount() const noexcept;
		const MaterialData& GetMaterialData(UINT materialIndex) const;

		UINT GetVertexStride() const noexcept;

		const AxisAlignedBox& GetBoundingBox() const noexcept;

		MeshRender& GetMeshRenderer(UINT meshRendererIndex) const;

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
		DX12Device* m_pDevice = nullptr;

		MeshData	m_meshData{};
		MaterialData*	m_pMaterialData = nullptr;
		LoadedModel*		m_pMesh = nullptr;
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