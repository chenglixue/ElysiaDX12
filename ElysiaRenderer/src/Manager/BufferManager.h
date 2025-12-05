#pragma once
#include "lib/Utility/Helper.h"


#include "IManager.h"
#include "IUpdate.h"
#include "lib/DX12/DX12TextureBuffer.h"
#include "lib/Utility/RenderTexture.h"


namespace ElysiaRenderer
{
	class DX12Device;
	struct BufferCreationDesc;
}

namespace ElysiaRenderer
{
	using namespace ElysiaHelper;

	class BufferManager : public IManager, IUpdate
	{
	public:
		BufferManager() = default;
		BufferManager(const BufferManager& rhs) = delete;
		BufferManager& operator=(BufferManager& rhs) = delete;
		BufferManager(BufferManager&& rhs) = default;
		~BufferManager();

		static BufferManager& GetInstance()
		{
			std::call_once(m_initInstanceFlag, []() {
				m_instance.reset(new BufferManager());
				});

			return *m_instance;
		}

		virtual void Init(DX12Device* pDevice) override;
		virtual void Destory() override;
		virtual void Update() override;

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
		DX12Device* m_pDevice = nullptr;
		static std::unique_ptr<BufferManager> m_instance;
		static std::once_flag m_initInstanceFlag;

		std::vector<std::unique_ptr<DX12BufferResource>> m_objectConstantBuffers{};
		std::unique_ptr<DX12BufferResource> m_pPassConstantBuffer = nullptr;
		std::unique_ptr<DX12BufferResource> m_pFrameConstantBuffer = nullptr;

		std::unique_ptr<RenderTexture> m_pCameraColorRT = nullptr;
		std::unique_ptr<RenderTexture> m_pCameraDepthRT = nullptr;
		
		std::unique_ptr<DX12BufferResource> m_pVertexBuffer = nullptr;
		std::unique_ptr<DX12BufferResource> m_pIndexBuffer = nullptr;
		D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView{};
		D3D12_INDEX_BUFFER_VIEW m_indexBufferView{};
	};
}