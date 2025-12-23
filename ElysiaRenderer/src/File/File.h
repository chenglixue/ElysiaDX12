#pragma once
#include "Programs/Helper.h"

namespace ElysiaHelper
{
	enum class FileOpenMode
	{
		Read = 0,
		Write = 1,
	};

	class File
	{
	public:
		File();
		File(const wchar_t* filePath, FileOpenMode openMode);
		~File();

		void Open(const wchar_t* filePath, FileOpenMode openMode);
		void Close();

		void Read(UINT64 size, void* data) const;
		void Write(UINT64 size, const void* data) const;

		template<typename T> void Read(T& data) const;
		template<typename T> void Write(const T& data) const;

		UINT64 Size() const;

	private:
		HANDLE m_fileHandle;
		FileOpenMode m_openMode;
	};

	template<typename T>
	void ReadFromFile(const wchar_t* fileName, T& val);

	template<typename T>
	void WriteToFile(const wchar_t* fileName, const T& val);

	// Returns the contents of a file as a string
	inline static std::string ReadFileAsString(const wchar_t* filePath)
	{
		File file(filePath, FileOpenMode::Read);
		UINT64 fileSize = file.Size();

		std::string fileContents;
		fileContents.resize(size_t(fileSize), 0);
		file.Read(fileSize, &fileContents[0]);

		return fileContents;
	}

	// Writes the contents of a string to a file
	inline static void WriteStringAsFile(const wchar_t* filePath, const std::string& data)
	{
		File file(filePath, FileOpenMode::Write);
		file.Write(data.length(), data.c_str());
	}
}