#pragma once
#include <d3d12.h>

namespace ElysiaRenderer
{
	constexpr uint32_t NUM_BACK_BUFFERS = 3;
	constexpr uint32_t NUM_FRAMES_IN_FLIGHT = 2;
	constexpr uint32_t NUM_RTV_STAGING_DESCRIPTORS = 256;
	constexpr uint32_t NUM_DSV_STAGING_DESCRIPTORS = 32;
	constexpr uint32_t NUM_SRV_STAGING_DESCRIPTORS = 4096;
	constexpr uint32_t MAX_QUEUED_BARRIERS = 16;
}