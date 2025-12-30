#include "stdafx.h"
#include "File.h"

namespace ElysiaHelper
{
	File::File() :
		m_fileHandle(INVALID_HANDLE_VALUE), m_openMode(FileOpenMode::Read)
	{
		
	}

	File::File(const std::wstring& filePath, FileOpenMode openMode) :
		m_fileHandle(INVALID_HANDLE_VALUE), m_openMode(FileOpenMode::Read)
	{
		Open(filePath, m_openMode);
	}

	File::~File()
	{
		Close();
		assert(m_fileHandle == INVALID_HANDLE_VALUE);
	}

	void File::Open(const std::wstring& filePath, FileOpenMode openMode)
	{
		assert(m_fileHandle == INVALID_HANDLE_VALUE);
		m_openMode = openMode;

		if (m_openMode == FileOpenMode::Read)
		{
			assert(FileExists(filePath));

			//创建或者打开一个文件或者I/O设备
			m_fileHandle = CreateFile(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (m_fileHandle == INVALID_HANDLE_VALUE)
			{
				std::wstring errPrefix = std::wstring(L"Failed to open file") + filePath + L":\n";
				assert(false);
				ThrowRuntimeError(WstringToString(errPrefix));
			}
		}
		else
		{
			// If the exists, delete it
			if (FileExists(filePath))
			{
				ThrowIfFailed(DeleteFile(filePath.c_str()));
			}

			m_fileHandle = CreateFile(filePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
			if (m_fileHandle == INVALID_HANDLE_VALUE)
			{
				std::wstring errPrefix = std::wstring(L"Failed to open file") + filePath + L":\n";
				assert(false);
				ThrowRuntimeError(WstringToString(errPrefix));
			}
		}
	}

	void File::Close()
	{
		if (m_fileHandle == INVALID_HANDLE_VALUE)
		{
			return;
		}

		ThrowIfFailed(CloseHandle(m_fileHandle));

		m_fileHandle = INVALID_HANDLE_VALUE;
	}

	void File::Read(UINT64 size, void* data) const
	{
		assert(m_fileHandle != INVALID_HANDLE_VALUE);
		assert(m_openMode == FileOpenMode::Read);

		DWORD bytesRead = 0;
		ThrowIfFailed(ReadFile(m_fileHandle, data, static_cast<DWORD>(size), &bytesRead, NULL));
	}

	void File::Write(UINT64 size, const void* data) const
	{
		assert(m_fileHandle != INVALID_HANDLE_VALUE);
		assert(m_openMode == FileOpenMode::Write);

		DWORD bytesWrite = 0;
		ThrowIfFailed(WriteFile(m_fileHandle, data, static_cast<DWORD>(size), &bytesWrite, NULL));
	}

	template<typename T> 
	void File::Read(T& data) const
	{
		Read(sizeof(T), &data);
	}

	template<typename T>
	void File::Write(const T& data) const
	{
		Write(sizeof(T), &data);
	}

	UINT64 File::Size() const
	{
		assert(m_fileHandle != INVALID_HANDLE_VALUE);

		LARGE_INTEGER fileSize;
		ThrowIfFailed(GetFileSizeEx(m_fileHandle, &fileSize));

		return fileSize.QuadPart;
	}

	template<typename T>
	void ReadFromFile(const wchar_t* fileName, T& val)
	{
		File file(fileName, FileOpenMode::Read);
		file.Read(val);
	}

	template<typename T>
	void WriteToFile(const wchar_t* fileName, const T& val)
	{
		File file(fileName, FileOpenMode::Write);
		file.Write(val);
	}
}