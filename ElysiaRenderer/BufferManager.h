#pragma once
#include "IManager.h"
#include "Helper.h"

namespace ElysiaRenderer
{
	using namespace ElysiaHelper;

	class DX12Device;
	class DX12BufferResource;
	class RenderTexture;
	struct BufferCreationDesc;

	class BufferManager : public IManager
	{
	public:
		BufferManager() = default;
		BufferManager(const BufferManager& rhs) = delete;
		BufferManager& operator=(BufferManager& rhs) = delete;
		BufferManager(BufferManager&& rhs) = default;
		~BufferManager();

		virtual void Init() override;
		virtual void Destory() override;

		DX12BufferResource* GetSingleConstantBuffer(uint8_t spaceID) const noexcept;
		DX12BufferResource* GetMutilConstantBuffer(uint8_t spaceID, UINT frameID, UINT objectIndex) const noexcept;
		RenderTexture* GetCameraDepthRT() const noexcept;
		RenderTexture* GetCameraColorRT() const noexcept;
		DX12BufferResource* GetVertexBuffer() const noexcept;
		DX12BufferResource* GetIndexBuffer() const noexcept;
		const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const noexcept;
		const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const noexcept;

		void AddConstantBuffer(uint8_t spaceID, BufferCreationDesc createDesc);
		void AddVertexBuffer(BufferCreationDesc desc);
		void AddIndexBuffer(BufferCreationDesc desc);
		void SetVertexBufferView(const D3D12_VERTEX_BUFFER_VIEW& view);
		void SetIndexBufferView(const D3D12_INDEX_BUFFER_VIEW& view);

	private:
		std::vector<std::unique_ptr<DX12BufferResource>> m_objectConstantBuffers{};
		std::unique_ptr<DX12BufferResource> m_pPassConstantBuffer = nullptr;

		std::unique_ptr<RenderTexture> m_pCameraColorRT = nullptr;
		std::unique_ptr<RenderTexture> m_pCameraDepthRT = nullptr;
		
		std::unique_ptr<DX12BufferResource> m_pVertexBuffer = nullptr;
		std::unique_ptr<DX12BufferResource> m_pIndexBuffer = nullptr;
		D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView{};
		D3D12_INDEX_BUFFER_VIEW m_indexBufferView{};
	};

	extern std::unique_ptr<BufferManager> g_pBufferManager;

	inline static BufferManager* GetBufferManager()
	{
		if (g_pBufferManager == nullptr)
		{
			ThrowRuntimeError("null buffer manager");
		}
		return g_pBufferManager.get();
	}
}