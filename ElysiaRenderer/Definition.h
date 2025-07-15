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

	static const wchar_t* SHADER_SOURCE_PATH = L"Shaders/";
	static const wchar_t* SHADER_OUTPUT_PATH = L"Shaders/Complied/";
}