#pragma once

namespace ElysiaCore
{
	class DX12BufferResource;
	using BufferHandle = std::shared_ptr<DX12BufferResource>;

	enum GPUResourceState
	{
		NoInit,
		InUse,
		Free,
	};
	
	enum class GPUResourceType : uint8_t
	{
		None = 0,
		Buffer,
		Texture
	};

	enum class GPUResourceFlags : uint8_t
	{
		None = 0,
		CBV = 1,
		SRV = 2,
		UAV = 4,
	};

	enum class BufferAccessFlags : uint8_t
	{
		GPUOnly = 0,
		HostWritable = 1
	};
	
	struct BufferCreationDesc
	{
		std::wstring name;
		size_t stride = 0;
		size_t size = 0;
		GPUResourceFlags viewFlags = GPUResourceFlags::None;
		BufferAccessFlags accessFlags = BufferAccessFlags::GPUOnly;
		bool isRawAccess = false;
	};

	struct DX12BufferUpload
	{
		BufferHandle buffer = nullptr;
		std::unique_ptr<uint8_t[]> pBufferData = nullptr;
		size_t bufferDataSize = 0;
		std::function<void (DX12BufferUpload*)> onComplete; 
	};

	inline BufferAccessFlags operator&(BufferAccessFlags a, BufferAccessFlags b)
	{
		return static_cast<BufferAccessFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
	}

	inline BufferAccessFlags operator|(BufferAccessFlags a, BufferAccessFlags b)
	{
		return static_cast<BufferAccessFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
	}

	inline GPUResourceFlags operator|(GPUResourceFlags a, GPUResourceFlags b)
	{
		return static_cast<GPUResourceFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
	}

	inline GPUResourceFlags operator&(GPUResourceFlags a, GPUResourceFlags b)
	{
		return static_cast<GPUResourceFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
	}
}
