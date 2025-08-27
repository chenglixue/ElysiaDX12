#pragma once
#include <d3d12.h>

namespace ElysiaRenderer
{
	constexpr uint32_t NUM_BACK_BUFFERS = 2;
	constexpr uint32_t NUM_FRAMES_IN_FLIGHT = 2;
	constexpr uint32_t NUM_RTV_STAGING_DESCRIPTORS = 256;
	constexpr uint32_t NUM_DSV_STAGING_DESCRIPTORS = 32;
	constexpr uint32_t NUM_SRV_STAGING_DESCRIPTORS = 4096;
	constexpr uint32_t MAX_QUEUED_BARRIERS = 16;
	constexpr uint32_t NUM_RESERVED_SRV_DESCRIPTORS = 8192;
	constexpr uint32_t NUM_SRV_RENDER_PASS_USER_DESCRIPTORS = 65536;
	constexpr uint32_t INVALID_RESOURCE_TABLE_INDEX = UINT_MAX;
	constexpr uint32_t NUM_SAMPLER_DESCRIPTORS = 6;
	constexpr uint32_t MAX_TEXTURE_SUBRESOURCE_COUNT = 32;

	using SubResourceLayouts = std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, MAX_TEXTURE_SUBRESOURCE_COUNT>;

	static const wchar_t* SHADER_SOURCE_PATH = L"Shaders/";
	static const wchar_t* SHADER_OUTPUT_PATH = L"Shaders/Complied/";

#define D3D_COMPILE_STANDARD_FILE_INCLUDE ((ID3DInclude*)(UINT_PTR)1)
}