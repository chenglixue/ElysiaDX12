#pragma once

#include "stdafx.h"
#include "File.h"

namespace ElysiaHelper
{
	using namespace DirectX::SimpleMath;


	class FileReadSerializer
	{
	public:
		explicit FileReadSerializer(const std::wstring& path)
		{
			m_file.Open(path, FileOpenMode::Read);
		}

		template<typename T> 
		void SerializeItem(T& data)
		{
			m_file.Read(data);
		}

		void SerializeData(UINT64 size, void* data)
		{
			m_file.Read(size, data);
		}

		static bool IsReadSerializer() { return true; }
		static bool IsWriteSerializer() { return false; }

	private:
		File m_file;
	};

	class FileWriteSerializer
	{
	public:
		explicit FileWriteSerializer(const std::wstring& path)
		{
			m_file.Open(path, FileOpenMode::Write);
		}

		template<typename T> void SerializeItem(const T& data)
		{
			m_file.Write(data);
		}

		void SerializeData(UINT64 size, const void* data)
		{
			m_file.Write(size, data);
		}

		static bool IsReadSerializer() { return false; }
		static bool IsWriteSerializer() { return true; }

	private:
		File m_file;
	};

	// Trampoline functions
	// Specialized serializers
	// Trampoline functions
	template<typename TSerializer, typename TData>
	static inline void SerializeData(TSerializer& serializer, TData& data)
	{
		serializer.SerializeData(sizeof(TData), &data);
	}

	template<typename TSerializer>
	static inline void SerializeItem(TSerializer& serializer, UINT8& val)
	{
		serializer.SerializeItem(val);
	}

	template<typename TSerializer>
	static inline void SerializeItem(TSerializer& serializer, INT8& val)
	{
		serializer.SerializeItem(val);
	}

	template<typename TSerializer>
	static inline void SerializeItem(TSerializer& serializer, UINT16& val)
	{
		serializer.SerializeItem(val);
	}

	template<typename TSerializer>
	static inline void SerializeItem(TSerializer& serializer, INT16& val)
	{
		serializer.SerializeItem(val);
	}

	template<typename TSerializer>
	static inline void SerializeItem(TSerializer& serializer, UINT32& val)
	{
		serializer.SerializeItem(val);
	}

	template<typename TSerializer>
	static inline void SerializeItem(TSerializer& serializer, INT32& val)
	{
		serializer.SerializeItem(val);
	}

	template<typename TSerializer>
	static inline void SerializeItem(TSerializer& serializer, UINT64& val)
	{
		serializer.SerializeItem(val);
	}

	template<typename TSerializer>
	static inline void SerializeItem(TSerializer& serializer, INT64& val)
	{
		serializer.SerializeItem(val);
	}

	template<typename TSerializer>
	static inline void SerializeItem(TSerializer& serializer, float& val)
	{
		serializer.SerializeItem(val);
	}

	template<typename TSerializer>
	static inline void SerializeItem(TSerializer& serializer, double& val)
	{
		serializer.SerializeItem(val);
	}

	template<typename TSerializer>
	static inline void SerializeItem(TSerializer& serializer, Vector2& val)
	{
		serializer.SerializeItem(val.x);
		serializer.SerializeItem(val.y);
	}

	template<typename TSerializer>
	static inline void SerializeItem(TSerializer& serializer, Vector3& val)
	{
		serializer.SerializeItem(val.x);
		serializer.SerializeItem(val.y);
		serializer.SerializeItem(val.z);
	}

	template<typename TSerializer>
	static inline void SerializeItem(TSerializer& serializer, Vector4& val)
	{
		serializer.SerializeItem(val.x);
		serializer.SerializeItem(val.y);
		serializer.SerializeItem(val.z);
		serializer.SerializeItem(val.w);
	}
	template<typename TSerializer>
	static inline void SerializeData(TSerializer& serializer, void* data, UINT64 size)
	{
		serializer.SerializeData(size, data);
	}

	template<typename TSerializer, typename TValue>
	static inline void SerializeArray(TSerializer& serializer, TValue* array, UINT64 numElements)
	{
		for (UINT64 i = 0; i < numElements; ++i)
			SerializeData(serializer, array[i]);
	}

	template<typename TSerializer, typename TValue>
	static inline void BulkSerializeArray(TSerializer& serializer, TValue* array, UINT64 numElements)
	{
		
		SerializeData(serializer, array, sizeof(TValue) * numElements);
	}

	template<typename T>
	static inline void SerializeToFile(const wchar_t* filePath, const T& item)
	{
		FileWriteSerializer serializer(filePath);
		SerializeData(serializer, item);
	}

	static inline void SerializeDataToFile(const wchar_t* filePath, void* data, UINT64 size)
	{
		FileWriteSerializer serializer(filePath);

		SerializeData(serializer, data, size);
	}
}