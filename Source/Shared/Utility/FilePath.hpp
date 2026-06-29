#pragma once
#include <assert.h>
#include <stdarg.h>
#include <clocale>
#include "String.hpp"
#include "FileData.h"
#include "Reflection\SerializationOverride.h"
#include "Reflection\ReflectionDefines.h"

namespace Hail
{
	constexpr uint32_t MAX_FILE_LENGTH = 260;
	class FilePath;
	class MetaResource;
	class InOutStream;

#ifdef PLATFORM_WINDOWS

	constexpr const wchar_t* g_SourceSeparatorAndEnd = L"\\\0";
	constexpr const wchar_t g_End = L'\0';
	constexpr const wchar_t g_Wildcard = L'*';
	constexpr const wchar_t g_SourceSeparator = L'\\';
	constexpr const wchar_t g_Separator = L'/';

#endif


	class FileObject
	{
	public:
		FileObject();
		FileObject(const FileObject& otherObject);
		FileObject(const FilePath& filePath);
		FileObject(const wchar_t* const string, const FileObject& parentObject, CommonFileData fileData, bool isDirectory);
		FileObject(const char* const name, const char* extension, const FilePath& directoryLevel);

		FileObject& operator=(const FileObject& object);
		FileObject& operator=(const FileObject&& moveableObject) noexcept;
		const FileObject& operator=(FileObject& object);
		const FileObject& operator=(FileObject&& moveableObject) noexcept;
		bool operator==(const FileObject& a) const;
		bool operator!=(const FileObject& a) const;

		const WString256& Name() const { return m_name; }
		const WString256& ParentName() const { return m_parentName; }
		const WString64& Extension() const { return m_extension; }
		void SetExtension(WString64 newExtension);
		bool IsDirectory() const { return m_isDirectory; }
		bool IsValid() const;
		bool IsFile() const { return !m_isDirectory; }
		uint32_t Length() const { return m_name.Length(); }
		const CommonFileData& GetFileData() const { return m_fileData; }

		uint64_t GetCachedLastWriteFileTime() const;
		uint16_t GetDirectoryLevel() const { return m_directoryLevel; }
	private:
		void Reset();
		// Will insert the StringName for the extension of the file.
		void FindExtension();
		friend class FilePath;

		WString256 m_name;
		WString256 m_parentName;
		WString64 m_extension;
		CommonFileData m_fileData;
		bool m_isDirectory;
		uint16_t m_directoryLevel;
	};



	class FilePath
	{
	public:
		FilePath()
		{
			Reset();
		}

		FilePath(const char* const string)
		{
			Reset();
			size_t length = strlen(string);
			if (length < MAX_FILE_LENGTH && length != 0)
			{
				mbstowcs(m_data, string, length + 1);
				m_length = length;
				FindExtension(false);
			}
			else
			{
				//error
			}
		}
		FilePath(const wchar_t* const string)
		{
			Reset();
			size_t length = wcslen(string);
			if (length < MAX_FILE_LENGTH)
			{
				wcscpy_s(m_data, string);
				m_length = length;
				FindExtension(false);
			}
			else
			{
				//error
			}
		}
		FilePath(const FilePath& path)
		{
			wcscpy_s(m_data, path.m_data);
			m_object = path.m_object;
			m_length = path.m_length;
			m_isDirectory = path.m_isDirectory;
			m_directoryLevel = path.m_directoryLevel;
		}

		FilePath(FilePath&& moveablePath) noexcept
		{
			wcscpy_s(m_data, moveablePath);
			m_object = moveablePath.m_object;
			m_length = moveablePath.m_length;
			m_isDirectory = moveablePath.m_isDirectory;
			m_directoryLevel = moveablePath.m_directoryLevel;
		}
		operator const wchar_t* () const
		{
			return m_data;
		}
		operator wchar_t* ()
		{
			return m_data;
		}
		FilePath& operator=(const FilePath& path)
		{
			wcscpy_s(m_data, path.m_data);
			m_object = path.m_object;
			m_length = path.m_length;
			m_isDirectory = path.m_isDirectory;
			m_directoryLevel = path.m_directoryLevel;
			return *this;
		}
		FilePath& operator=(FilePath&& moveablePath) noexcept
		{
			wcscpy_s(m_data, moveablePath.m_data);
			m_object = moveablePath.m_object;
			m_length = moveablePath.m_length;
			m_isDirectory = moveablePath.m_isDirectory;
			m_directoryLevel = moveablePath.m_directoryLevel;
			return *this;
		}
		FilePath& operator=(const wchar_t* const string)
		{
			Reset();
			if (string)
				wcscpy_s(m_data, string);
			m_length = wcslen(m_data);
			if (string)
				FindExtension(false);
			return *this;
		}

		FilePath& operator+=(const FilePath& otherPath);
		FilePath& operator+=(const FileObject& object);
		FilePath& operator+=(const wchar_t* const string);
		FilePath Parent() const;
		//Either the file object or the path
		const FileObject& Object() const { return m_object; }
		const wchar_t* Data() const	{ return m_data; }

		void Slashify();

		bool IsDirectory() const { return m_isDirectory; }
		bool IsValid() const; 
		bool IsFile() const	{ return !m_isDirectory; }
		bool IsEmpty() const { return m_length == 0; }
		uint32_t Length() const { return m_length; }
		uint16_t GetDirectoryLevel() const { return m_directoryLevel; }
		FilePath GetDirectoryAtLevel(uint16 levelToGet) const;
		void AddWildcard();
		//Creates a directory if none exist, otherwise this function does nothing.
		bool CreateFileDirectory() const;

		//Fetches the file data from the path, the size and creation time.
		CommonFileData LoadCommonFileData() const;
		//Updates the filepaths file data.
		void UpdateCommonFileData();
		// Folder of the exe
		static const FilePath& GetCurrentWorkingDirectory();
		// Will be User / ProjectName for save files and the like
		static const FilePath& GetUserProjectDirectory();
		// Hail/Source/Resources/
		static const FilePath& GetResourceSourceDirectory();
		// Generated/bin/Scripts
		static const FilePath& GetAngelscriptDirectory();
		// Folder of shader resources for import
		static const FilePath& GetShaderResourceDirectory();
		// Folder of compiled shaders
		static const FilePath& GetShaderCompiledDirectory();
		// Folder of texture source files
		static const FilePath& GetTextureResourceSourceDirectory();
		// Imported textures
		static const FilePath& GetTextureCompiledDirectory();

		//returns -1 if no common directory is found
		static int16 FindCommonLowestDirectoryLevel(const FilePath& pathA, const FilePath& pathB);
		// Will check the actual file on disk to see when it was most recently written too.
		uint64_t GetCurrentLastWriteFileTime() const;

		void ReplaceExtension(WString64 newExtension);

	protected:
		FilePath(const FilePath& path, uint32_t lengthOfPath);
		void FindExtension(bool useFileObjectsExtension);
		void CreateFileObject();
		// If the filepath is a directory this will clear the final separator if one is present
		void DeleteEndSeperator();

	private:
		static FilePath ProjectCurrentWorkingDirectory;
		static FilePath UserProjectDirectory;
		static FilePath AngelscriptDirectory;
		static FilePath ShaderResourceDirectory;
		static FilePath ShaderCompiledDirectory;
		static FilePath TextureResourceSourceDirectory;
		static FilePath TextureCompiledDirectory;
		static FilePath ResourceSourceDirectory;
		friend class RelativeFilePath;
		void Reset();
		wchar_t m_data[MAX_FILE_LENGTH];
		FileObject m_object;
		uint32_t m_length = 0;
		bool m_isDirectory = false;
		uint16_t m_directoryLevel = 0;
	};

	inline FilePath operator+(const FilePath& lhs, const FilePath& rhs)
	{
		FilePath returnPath = lhs;
		returnPath += rhs;
		return returnPath;
	}

	inline FilePath operator+(const FilePath& lhs, const FileObject& rhs)
	{
		FilePath returnPath = lhs;
		returnPath += rhs;
		return returnPath;
	}
	inline FilePath operator+(const FilePath& lhs, const wchar_t* const string)
	{
		FilePath returnPath = lhs;
		returnPath += string;
		return returnPath;
	}

	static bool operator<(const FilePath& path1, const FilePath& path2)
	{
		return path1.Length() < path2.Length();
	}
	static bool operator>(const FilePath& path1, const FilePath& path2)
	{
		return path1.Length() > path2.Length();
	}
	static bool operator==(const FilePath& path1, const FilePath& path2)
	{
		if (path1.Length() == path2.Length() && 
			path1.IsDirectory() == path2.IsDirectory())
		{
			return wcscmp(path1.Data(), path2.Data()) == 0;
		}
		return false;
	}
	static bool operator!=(const FilePath& path1, const FilePath& path2)
	{
		if (path1.Length() == path2.Length() &&
			path1.IsDirectory() == path2.IsDirectory())
		{
			return wcscmp(path1.Data(), path2.Data()) != 0;
		}
		return true;
	}

	// Filepath object that is always relative to the working directory / bin folder
	class RelativeFilePath : public SerializeableObjectCustom
	{
	public:
		RelativeFilePath();
		explicit RelativeFilePath(const FilePath& longFilePath);
		explicit RelativeFilePath(const char* relativeProjectPath);
		//Constructs a FilePath object from the known Data
		FilePath GetFilePath() const;
		const wchar_t* GetRelativePathData() const { return m_pathFromWorkingDir; }
		bool Empty() const { return m_pathLength == 0; }
		void Serialize(InOutStream& outObject) override;
		void Deserialize(InOutStream& inObject) override;
		StringL GetName() const { return m_name; }
	private:
		int16 m_stepsFromFileToCommonSharedDir;
		uint16 m_directoryLevel;
		bool m_isInsideWorkingDirectory;

		wchar_t m_pathFromWorkingDir[MAX_FILE_LENGTH];
		uint16 m_pathLength;
		StringL m_name;
	};
}
