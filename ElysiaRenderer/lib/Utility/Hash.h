#pragma once
#include <stdint.h>
#include <type_traits>
#include <functional>

namespace xxh
{
	size_t xxhash_gethash(void const* ptr, size_t sz);
	//Size must less than 32 in x64
	size_t xxhash_gethash_small(void const* ptr, size_t sz);

	template<typename T>
	requires(std::is_trivial_v<T> && (!std::is_reference_v<T>))
	size_t GetHash(T const& v)
	{
		if constexpr (sizeof(T) < 32)
		{
			return xxh::xxhash_gethash_small(&v, sizeof(T));
		}
		else
		{
			return xxh::xxhash_gethash(&v, sizeof(T));
		}
	}


	inline size_t GetHash(const void* data, size_t size)
	{
		if (size < 32)
		{
			return xxh::xxhash_gethash_small(data, size);
		}
		else
		{
			return xxh::xxhash_gethash(data, size);
		}
	}

	inline size_t GetHash(const std::string& str)
	{
		return GetHash(str.data(), str.size());
	}
}