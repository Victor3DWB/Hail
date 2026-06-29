#include "Shared_PCH.h"
#include "FilePath.hpp"

#include "DebugMacros.h"
#include "MathUtils.h"
#include "InOutStream.h"

#ifdef PLATFORM_WINDOWS

#include <windows.h>
#include <shlobj.h>

#pragma comment(lib, "shell32.lib")

//#elif PLATFORM_OSX//.... more to be added

#endif

#include "Utility\StringUtility.h"

#include "Reflection\Reflection.h"
//Reflection code inserted below, do not add or modify code in the namespace Reflection
namespace Hail::Reflection
{
    DEFINE_TYPE_CUSTOM_SERIALIZER(RelativeFilePath)
}

using namespace Hail;

namespace
{
    int GetParentSeperator(const wchar_t* path, uint32_t length, bool isDirectory)
    {
        if (length == 0)
        {
            return 0;
        }
        int32_t seperator = length - 1;
        wchar_t currentCharacter = 0;
        bool foundFirstSeperator = false;
        while (seperator != -1)
        {
            if (isDirectory && !foundFirstSeperator)
            {
                if (currentCharacter == Hail::g_SourceSeparator)
                {
                    currentCharacter = L'\0';
                    foundFirstSeperator = true;
                }
            }
            if (currentCharacter == Hail::g_SourceSeparator)
            {
                return length - (seperator + 1);
                break;
            }
            currentCharacter = path[seperator--];
        }
        return -1;
    }
}

FilePath FilePath::ProjectCurrentWorkingDirectory("");
FilePath FilePath::UserProjectDirectory("");
FilePath FilePath::ResourceSourceDirectory("");
FilePath FilePath::AngelscriptDirectory("");
FilePath FilePath::ShaderResourceDirectory("");
FilePath FilePath::ShaderCompiledDirectory("");
FilePath FilePath::TextureResourceSourceDirectory("");
FilePath FilePath::TextureCompiledDirectory("");

void FilePath::CreateFileObject()
{
    m_object = FileObject(*this);
}

FilePath FilePath::GetDirectoryAtLevel(uint16 levelToGet) const
{
    if (levelToGet >= m_directoryLevel)
        return *this;

    wchar_t data[MAX_FILE_LENGTH];
    uint16 levelCounter = 0xffff;
    for (size_t i = 0; i < m_length; i++)
    {
        if (m_data[i] == g_SourceSeparator)
        {
            levelCounter++;
        }
        data[i] = m_data[i];
        if (levelCounter == levelToGet)
        {
            data[i + 1] = g_End;
            break;
        }
    }
    return FilePath(data);
}

void FilePath::AddWildcard()
{
    if (m_isDirectory && m_data[m_length - 1] != g_Wildcard)
    {
        m_data[m_length++] = g_Wildcard;
        m_data[m_length] = L'\0';
    }
}

bool Hail::FilePath::CreateFileDirectory() const
{
    FilePath pathToCreate = *this;
    if (!m_isDirectory)
    {
        pathToCreate = Parent();
    }
    if (!pathToCreate.IsValid())
    {
        uint16_t directoryLevelWhereParentExist = 0;
        {
            Hail::FilePath parentPath = pathToCreate.Parent();
            while (!parentPath.IsValid())
            {
                parentPath = parentPath.Parent();
                //There is no valid parent path, meaning the directory is entirely invalid
                if (parentPath.GetDirectoryLevel() == 1 && !parentPath.IsValid())
                {
                    return false;
                }
            }
            directoryLevelWhereParentExist = parentPath.GetDirectoryLevel();
        }

        //construct the entire folder hierarchy
        for (size_t i = pathToCreate.GetDirectoryLevel() - directoryLevelWhereParentExist; i > 0; i--)
        {
            pathToCreate = *this;
            while (pathToCreate.GetDirectoryLevel() != directoryLevelWhereParentExist + i)
            {
                pathToCreate = pathToCreate.Parent();
            }
#ifdef PLATFORM_WINDOWS
            if (CreateDirectory(pathToCreate.Data(), NULL) ||
                ERROR_ALREADY_EXISTS == GetLastError())
            {
                directoryLevelWhereParentExist++;
            }
            else
            {
                // Failed to create directory.
                std::cout << " Failed to create Directory! " << std::endl;
            }
#endif
        }
    }

    return true;
}
CommonFileData FilePath::LoadCommonFileData() const
{
    return ConstructFileDataFromPath(*this);
}

void Hail::FilePath::UpdateCommonFileData()
{
    m_object.m_fileData = LoadCommonFileData();
}

const FilePath& FilePath::GetCurrentWorkingDirectory()
{
    if (ProjectCurrentWorkingDirectory.Length() != 0)
        return ProjectCurrentWorkingDirectory;

    wchar_t buffer[MAX_FILE_LENGTH];
#ifdef PLATFORM_WINDOWS
   if (GetCurrentDirectory(MAX_FILE_LENGTH, buffer) == NULL) 
   {
       //TODO: add assert
       perror("getcwd() error");
   }
#endif
   ProjectCurrentWorkingDirectory = FilePath(buffer);
   return ProjectCurrentWorkingDirectory;
}

const FilePath& Hail::FilePath::GetUserProjectDirectory()
{
    if (UserProjectDirectory.Length() != 0)
        return UserProjectDirectory;

    WString64 applicationName = L"Hail"; // TODO: exchange with our own chosen name
    wchar_t data[MAX_FILE_LENGTH];
    bool foundPath = false;

#ifdef PLATFORM_WINDOWS
    PWSTR   ppszPath;    // variable to receive the path memory block pointer.

    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &ppszPath);

    if (SUCCEEDED(hr)) 
    {
        memcpy(data, ppszPath, StringLength(ppszPath) * sizeof(wchar_t));
        data[StringLength(ppszPath)] = g_End;
        foundPath = true;
    }

    CoTaskMemFree(ppszPath);
#endif
    if (foundPath)
    {
        UserProjectDirectory = FilePath(data) + applicationName.Data();
    }

    return UserProjectDirectory;
}

const FilePath& Hail::FilePath::GetResourceSourceDirectory()
{
    if (ResourceSourceDirectory.Length() != 0)
        return ResourceSourceDirectory;

    ResourceSourceDirectory = GetCurrentWorkingDirectory().Parent().Parent() + L"Source/Resources/";
    return ResourceSourceDirectory;
}

const FilePath& FilePath::GetAngelscriptDirectory()
{
    if (AngelscriptDirectory.Length() != 0)
        return AngelscriptDirectory;

    AngelscriptDirectory = GetCurrentWorkingDirectory() + L"scripts/";
    return AngelscriptDirectory;
}

const FilePath& Hail::FilePath::GetShaderResourceDirectory()
{
    if (ShaderResourceDirectory.Length() != 0)
        return ShaderResourceDirectory;

    ShaderResourceDirectory = GetCurrentWorkingDirectory().Parent().Parent() + L"Source/Resources/Shaders/";
    return ShaderResourceDirectory;
}

const FilePath& Hail::FilePath::GetShaderCompiledDirectory()
{
    if (ShaderCompiledDirectory.Length() != 0)
        return ShaderCompiledDirectory;

    ShaderCompiledDirectory = GetCurrentWorkingDirectory() + L"resources/shaders/";
    return ShaderCompiledDirectory;
}

const FilePath& Hail::FilePath::GetTextureResourceSourceDirectory()
{
    if (TextureResourceSourceDirectory.Length() != 0)
        return TextureResourceSourceDirectory;

    TextureResourceSourceDirectory = GetCurrentWorkingDirectory().Parent().Parent() + L"Source/Resources/Textures/";
    return TextureResourceSourceDirectory;
}

const FilePath& Hail::FilePath::GetTextureCompiledDirectory()
{
    if (TextureCompiledDirectory.Length() != 0)
        return TextureCompiledDirectory;

    TextureCompiledDirectory = GetCurrentWorkingDirectory() + L"resources/textures/";
    return TextureCompiledDirectory;
}

int16 Hail::FilePath::FindCommonLowestDirectoryLevel(const FilePath& pathA, const FilePath& pathB)
{
    if (!pathA.IsValid() || !pathB.IsValid())
        return -1;

    int16 commonLowestDirectory = -1;
    for (size_t i = 0; i < pathA.m_length; i++)
    {
        // Windows filesystem is not case sensitive, TODO: if this is not windows there will be bugs here.
        if (std::tolower(pathA.m_data[i]) != std::tolower(pathB.m_data[i]))
        {
            break;
        }
        if (pathA.m_data[i] == g_SourceSeparator)
        {
            commonLowestDirectory++;
        }
    }
    return commonLowestDirectory;
}

uint64_t Hail::FilePath::GetCurrentLastWriteFileTime() const
{
    CommonFileData fileData = LoadCommonFileData();
    uint64_t returnValue;
    returnValue = fileData.m_lastWriteTime.m_highDateTime;
    returnValue << 32;
    returnValue |= fileData.m_lastWriteTime.m_lowDateTime;
    return returnValue;
}

void Hail::FilePath::DeleteEndSeperator()
{
    if (!m_isDirectory)
        return;
    const wchar_t lastDigit = m_data[m_length - 1];
    if (lastDigit == g_SourceSeparator)
        m_data[--m_length] = L'\0';
}

Hail::FilePath::FilePath(const FilePath& path, uint32_t lengthOfPath)
{
    *this = path;
    if (m_directoryLevel == 1)
        --lengthOfPath;

    for (; ; --m_length)
    {
        m_data[m_length] = L'\0';
        lengthOfPath--;
        if (lengthOfPath == 0)
        {
            break;
        }
    }
    FindExtension(false);
}

Hail::FilePath& Hail::FilePath::operator+=(const FilePath& otherPath)
{
    m_object = otherPath.Object();
    if (otherPath.IsValid())
    {
        if (m_isDirectory)
        {
            if (m_data[m_length - 1] == g_Wildcard)
            {
                m_data[--m_length] = L'\0';
                wcscat_s(m_data, Hail::MAX_FILE_LENGTH - m_length, otherPath.Data());
                m_length += otherPath.Length();
            }
            else
            {
                //memcpy(m_data + m_length, otherPath.Data(), otherPath.Length());
                wcscat_s(m_data, MAX_FILE_LENGTH - m_length, otherPath.Data());
            }
        }
        else
        {
            wcscat_s(m_data, MAX_FILE_LENGTH - m_length, otherPath.Data());
        }
        FindExtension(true);
    }
    return *this;
}

Hail::FilePath& Hail::FilePath::operator+=(const FileObject& object)
{
    const bool bSameDirectoryLevel = object.GetDirectoryLevel() == GetDirectoryLevel() && GetDirectoryLevel() != 0;
    if (bSameDirectoryLevel && Object() == object)
    {
        return *this;
    }

    uint32_t length = 0;
    m_object = object;
    if (object.IsValid() && m_isDirectory)
    {
        if (m_length != 0 && m_data[m_length - 1] == g_Wildcard)
        {
            m_data[--m_length] = L'\0';
        }
        length += object.Name().Length();
        wcscat_s(m_data, MAX_FILE_LENGTH - m_length, object.Name());
        if (object.IsDirectory())
        {
            m_data[m_length + length++] = g_SourceSeparator;
        }
        else
        {
            m_data[m_length + length++] = L'.';
            m_data[m_length + length] = g_End;
            length += object.Extension().Length();
            wcscat_s(m_data, MAX_FILE_LENGTH - (m_length + length), object.Extension());
        }
        m_data[m_length + length] = g_End;
        m_length += length;

        FindExtension(true);
    }
    return *this;
}

Hail::FilePath& Hail::FilePath::operator+=(const wchar_t* const string)
{
    uint32_t length = wcslen(string);
    if (length > 0)
    {
        if (m_isDirectory)
        {
            if (m_data[m_length - 1] == g_Wildcard)
            {
                m_data[--m_length] = L'\0';
                wcscat_s(m_data, Hail::MAX_FILE_LENGTH - m_length, string);
            }
            else
            {
                wcscat_s(m_data, MAX_FILE_LENGTH - m_length, string);
            }
        }
        else
        {
            wcscat_s(m_data, MAX_FILE_LENGTH - m_length, string);
        }
        m_length += length;
        Slashify();
        FindExtension(false);
    }
    return *this;
}

Hail::FilePath Hail::FilePath::Parent() const
{
    if (m_directoryLevel >= 1)
    {
        const int objectLength = GetParentSeperator(m_data, m_length, m_isDirectory);
        FilePath path = FilePath(*this, objectLength + 1);
        return path;
    }
    else
    {
        //No parent directory
        return *this;
    }
}


void Hail::FilePath::Slashify()
{
    size_t size = StringLength(m_data);
    if (size == 0)
    {
        return;
    }
    if (m_data[size - 1] == g_SourceSeparator || m_data[size - 1] == g_Separator)
    {
        m_data[size - 1] = g_End;
        size--;
    }

    m_directoryLevel = 0;
    wchar_t temp[MAX_FILE_LENGTH];
    temp[0] = L'\0';
    wcscpy_s(temp, m_data);
    uint32_t counter = 0;
    size_t i(0);
    for (; i < size; i++)
    {
        if ((temp[i] == g_SourceSeparator || temp[i] == g_Separator) && temp[i + 1] != g_SourceSeparator && temp[i + 1] != g_Separator)
        {
            m_data[counter++] = g_SourceSeparator;
            m_directoryLevel++;
        }
        else if (m_data[i] != g_SourceSeparator && m_data[i] != g_Separator)
        {
            m_data[counter++] = temp[i];
        }
    }

    if (m_isDirectory)
    {
        if (m_data[counter - 1] != g_SourceSeparator && m_data[counter - 1] != L'*')
        {
            m_data[counter] = g_SourceSeparator;
            m_data[++counter] = L'\0';
        }
    }
    m_length = counter;
}

void Hail::FilePath::ReplaceExtension(WString64 newExtension)
{
    if (m_isDirectory || m_length == 0u)
        return;

    memset(&m_data[m_length - m_object.Extension().Length()], 0, m_object.Extension().Length());
    m_length -= m_object.Extension().Length();

    memcpy(&m_data[m_length], newExtension.Data(), newExtension.Length() * sizeof(wchar_t));
    m_length += newExtension.Length();

    m_object.SetExtension(newExtension);
}


bool Hail::FilePath::IsValid() const
{
    if (m_length == 0)
    {
        return false;
    }
    return IsValidFilePathInternal(this);
}

void Hail::FilePath::FindExtension(bool useFileObjectsExtension)
{
    if (m_length == 0)
    {
        return;
    }

    if (useFileObjectsExtension)
    {
        m_isDirectory = m_object.IsDirectory();
    }
    else
    {
        m_isDirectory = true;
        int32_t seperator = m_length - 1;
        wchar_t currentCharacter = 0;
        while (seperator != 0)
        {
            if (currentCharacter == g_Wildcard)
            {
                break;
            }
            if (currentCharacter == L'.')
            {
                m_isDirectory = false;
                break;
            }
            currentCharacter = m_data[seperator--];
        }
    }

    Slashify();
    CreateFileObject();
}

void Hail::FileObject::SetExtension(WString64 newExtension)
{
    if (!m_isDirectory)
    {
        m_extension = newExtension;
    }
}

bool Hail::FileObject::IsValid() const
{
    return m_name.Length() != 0;
}

void Hail::FilePath::Reset()
{
    m_data[0] = L'\0';
    m_length = 0;
    m_isDirectory = true;
    m_directoryLevel = 0;
}

//Hail::FilePath operator+(const Hail::FilePath& string1, const Hail::FilePath& string2)
//{
//    Hail::FilePath path = string1;
//    if (string1.IsDirectory())
//    {
//        if (path.Data()[path.Length() - 1] == L'*')
//        {
//            path.DataRef()[path.Length() - 1] = L'\0';
//            wcscat_s(path, Hail::MAX_FILE_LENGTH - string1.Length(), string2.Data());
//        }
//        else
//        {
//            //wcscat_s(path, MAX_FILE_LENGTH - string1.Length(), string2.Data());
//            memcpy(path.DataRef() + string1.Length(), string2.Data(), string2.Length());
//        }
//    }
//    path.FindExtension();
//    return path;
//}

Hail::FileObject::FileObject()
{
    Reset();
}

Hail::FileObject::FileObject(const FileObject& otherObject) : 
    m_name(otherObject.m_name),
    m_parentName(otherObject.m_parentName)
{
    m_extension = otherObject.m_extension;
    m_isDirectory = otherObject.m_isDirectory;
    m_fileData = otherObject.m_fileData;
    m_directoryLevel = otherObject.m_directoryLevel;
}

Hail::FileObject::FileObject(const FilePath& filePath)
{
    Reset();
    if(filePath.Length() == 0)
    {
        return;
    }

    if (filePath.GetDirectoryLevel() < 1)
    {
        m_name.Data()[0] = filePath.Data()[0];
        m_name.Data()[1] = L':';
        m_name.Data()[2] = g_End;
        m_directoryLevel = 0;
        m_isDirectory = true;
        const wchar_t* folderName = L"folder\0";
        memcpy(m_extension, folderName, sizeof(wchar_t) * 7);
        return;
    }

    const int32_t parentSeperator = GetParentSeperator(filePath.Data(), filePath.Length(), filePath.IsDirectory());
    m_name = filePath.Data() + (filePath.Length() - parentSeperator) + 1;
    const int32_t length = m_name.Length();
    if (filePath.IsDirectory())
    {
        if (filePath.Data()[filePath.Length() - 1] == g_Wildcard)
        {
            m_name.Data()[length - 2] = g_End;
            //m_length--;
        }
        m_name.Data()[length - 1] = g_End;
        m_isDirectory = true;
        const wchar_t* folderName = L"folder\0";
        memcpy(m_extension, folderName, sizeof(wchar_t) * 7);
        //m_data[--m_length] = L'\0';
    }
    else
    {
        int32_t seperator = m_name.Length();
        wchar_t currentCharacter = 0;
        while (seperator != 0)
        {
            if (currentCharacter == g_Wildcard)
            {
                break;
            }
            if (currentCharacter == L'.')
            {
                m_isDirectory = false;
                seperator++;
                break;
            }
            currentCharacter = m_name[seperator--];
        }
        const uint32_t fileExtensionLength = m_name.Length() != 0 ? m_name.Length() - (seperator + 1) : 0;
        memcpy(m_extension, m_name + seperator + 1, sizeof(wchar_t) * fileExtensionLength);
        m_extension[fileExtensionLength] = g_End;
        memset(m_name + seperator, 0, sizeof(wchar_t) * (fileExtensionLength + 1));
    }

    int32_t parentParentSeperator = GetParentSeperator(filePath.Data(), (filePath.Length() - parentSeperator) + 1, true);
    if (parentParentSeperator != -1)
    {
        memcpy(m_parentName, filePath.Data() + ((filePath.Length() - parentSeperator) - parentParentSeperator) + 2, sizeof(wchar_t) * (parentParentSeperator));
        m_parentName[parentParentSeperator - 2] = g_End;
    }
    //Parent is Root directory
    else
    {
        m_parentName[0] = filePath.Data()[0];
        m_parentName[1] = L'\0';
    }

    m_directoryLevel = filePath.GetDirectoryLevel();
    m_fileData = ConstructFileDataFromPath(filePath);
}

Hail::FileObject::FileObject(const wchar_t* const string, const FileObject& parentObject, CommonFileData fileData, bool isDirectory)
{
    Reset();
    m_fileData = fileData;
    m_name = string;
    m_parentName = parentObject.Name();
    m_directoryLevel = parentObject.GetDirectoryLevel() + 1;
    m_isDirectory = isDirectory;
    FindExtension();
}

Hail::FileObject::FileObject(const char* const name, const char* extension, const FilePath& directoryLevel)
{
    FileObject parentObject;
    m_directoryLevel = directoryLevel.GetDirectoryLevel();
    if (directoryLevel.IsDirectory())
    {
        m_directoryLevel++;
        parentObject = directoryLevel.Object();
    }
    else
    {
        parentObject = directoryLevel.Parent().Object();
    }
    memcpy(m_parentName, parentObject.m_name, sizeof(m_parentName));

    FromConstCharToWChar(name, m_name, 64);
    FromConstCharToWChar(extension, m_extension, 64);
    m_isDirectory = false;

}

Hail::FileObject& Hail::FileObject::operator=(const FileObject& object)
{
    m_name = object.m_name;
    m_parentName = object.m_parentName;
    m_extension = object.m_extension;
    m_isDirectory = object.m_isDirectory;
    m_fileData = object.m_fileData;
    m_directoryLevel = object.m_directoryLevel;
    return *this;
}

Hail::FileObject& Hail::FileObject::operator=(const FileObject&& moveableObject) noexcept
{
    m_name = moveableObject.m_name;
    m_parentName = moveableObject.m_parentName;
    m_extension = moveableObject.m_extension;
    m_isDirectory = moveableObject.m_isDirectory;
    m_fileData = moveableObject.m_fileData;
    m_directoryLevel = moveableObject.m_directoryLevel;
    return *this;
}

const Hail::FileObject& Hail::FileObject::operator=(FileObject& object)
{
    m_name = object.m_name;
    m_parentName = object.m_parentName;
    m_extension = object.m_extension;
    m_isDirectory = object.m_isDirectory;
    m_fileData = object.m_fileData;
    m_directoryLevel = object.m_directoryLevel;
    return *this;
}

const Hail::FileObject& Hail::FileObject::operator=(FileObject&& moveableObject) noexcept
{
    m_name = moveableObject.m_name;
    m_parentName = moveableObject.m_parentName;;
    m_extension = moveableObject.m_extension;
    m_isDirectory = moveableObject.m_isDirectory;
    m_fileData = moveableObject.m_fileData;
    m_directoryLevel = moveableObject.m_directoryLevel;
    return *this;
}

bool Hail::FileObject::operator==(const FileObject& a) const
{
    if (a.m_directoryLevel != m_directoryLevel || a.m_isDirectory != m_isDirectory)
        return false;
    if (a.m_extension != m_extension)
        return false;
    if (a.m_parentName != m_parentName)
        return false;
    if (a.m_name != m_name)
        return false;
    return true;
}

bool Hail::FileObject::operator!=(const FileObject& a) const
{
    return !(a == *this);
}

uint64_t Hail::FileObject::GetCachedLastWriteFileTime() const
{
    uint64_t returnValue;
    returnValue = m_fileData.m_lastWriteTime.m_highDateTime;
    returnValue << 32;
    returnValue |= m_fileData.m_lastWriteTime.m_lowDateTime;
    return returnValue;
}

void Hail::FileObject::Reset()
{
    m_name = L'\0';
    m_parentName = L'\0';
    m_extension[0] = L'\0';
    //m_length = 0;
    m_isDirectory = false;
    m_directoryLevel = 0;
    memset(&m_fileData, 0, sizeof(CommonFileData));
}

void Hail::FileObject::FindExtension()
{
    const uint32_t length = m_name.Length();
    if (length == 0)
    {
        return;
    }
    if (m_isDirectory)
    {
        m_extension = L"folder\0";
        return;
    }

    int32_t seperator = length - 1;
    wchar_t currentCharacter = 0;
    while (seperator != 0)
    {
        if (currentCharacter == L'.')
        {
            break;
        }
        currentCharacter = m_name[seperator--];
    }
    if (m_isDirectory == false)
    {
        seperator++;
        m_extension = &m_name[seperator + 1];
        //m_extension[length - (seperator + 1)] = L'\0';
        memset(m_name + seperator, 0, sizeof(wchar_t) * (length - seperator));
        //m_length = seperator;
        return;
    }
    return;
}

RelativeFilePath::RelativeFilePath()
{
    m_stepsFromFileToCommonSharedDir = -1;
    m_isInsideWorkingDirectory = false;
    m_directoryLevel = 0;
    m_pathLength = 0;
    m_pathFromWorkingDir[0] = g_End;
}

RelativeFilePath::RelativeFilePath(const FilePath& longFilePath)
{
    const FilePath& currentWorkingDirectory = FilePath::GetCurrentWorkingDirectory();
    const uint16 currentWorkingDirectoryLevel = currentWorkingDirectory.m_directoryLevel;
    bool isDirectoryInsideOfWorkingDirectory = false;

    m_directoryLevel = longFilePath.GetDirectoryLevel();

    //if file is in a higher level we could be inside of the working directory
    if (currentWorkingDirectoryLevel < longFilePath.GetDirectoryLevel())
    {
        FilePath filePathAtDirectoryLevel = longFilePath.GetDirectoryAtLevel(currentWorkingDirectoryLevel);

        if (StringCompare(filePathAtDirectoryLevel.m_data, currentWorkingDirectory.m_data))
        {
            isDirectoryInsideOfWorkingDirectory = true;
        }

        if (isDirectoryInsideOfWorkingDirectory)
        {
            m_stepsFromFileToCommonSharedDir = longFilePath.GetDirectoryLevel() - currentWorkingDirectoryLevel;

            const uint16 nameLengthToDirectory = StringLength(filePathAtDirectoryLevel.m_data);
            m_pathLength = longFilePath.Length() - nameLengthToDirectory;
            memcpy(m_pathFromWorkingDir, &longFilePath.m_data[nameLengthToDirectory], m_pathLength * sizeof(wchar_t));
            m_pathFromWorkingDir[m_pathLength] = g_End;
            m_isInsideWorkingDirectory = true;
            m_name = longFilePath.Object().Name().ToCharString().Data();
            return;
        }
    }

    m_isInsideWorkingDirectory = false;
    // find common lowest directory and set the data
    const int16 lowestCommonDirectoryLevel = FilePath::FindCommonLowestDirectoryLevel(longFilePath, currentWorkingDirectory);

    if (lowestCommonDirectoryLevel == -1)
    {
        m_stepsFromFileToCommonSharedDir = -1;
        m_pathLength = longFilePath.Length();
        memcpy(m_pathFromWorkingDir, &longFilePath.m_data, m_pathLength * sizeof(wchar_t));
    }
    else
    {
        m_stepsFromFileToCommonSharedDir = longFilePath.GetDirectoryLevel() - lowestCommonDirectoryLevel;
        FilePath filePathAtCommonDirectoryLevel = longFilePath.GetDirectoryAtLevel(lowestCommonDirectoryLevel);
        const uint16 nameLengthToDirectory = StringLength(filePathAtCommonDirectoryLevel.m_data);
        m_pathLength = longFilePath.Length() - nameLengthToDirectory;
        memcpy(m_pathFromWorkingDir, &longFilePath.m_data[nameLengthToDirectory], m_pathLength * sizeof(wchar_t));
        m_pathFromWorkingDir[m_pathLength] = g_End;
    }
    m_name = longFilePath.Object().Name().ToCharString().Data();
}

RelativeFilePath::RelativeFilePath(const char* relativeProjectPath)
{
    const FilePath& currentWorkingDirectory = FilePath::GetCurrentWorkingDirectory();
    const uint16 currentWorkingDirectoryLevel = currentWorkingDirectory.m_directoryLevel;
    bool isDirectoryInsideOfWorkingDirectory = false;

    StringLW path = relativeProjectPath;
    FilePath longFilePath = currentWorkingDirectory;
    longFilePath = longFilePath + path.Data();

    m_directoryLevel = longFilePath.GetDirectoryLevel();

    //if file is in a higher level we could be inside of the working directory
    if (currentWorkingDirectoryLevel < longFilePath.GetDirectoryLevel())
    {
        FilePath filePathAtDirectoryLevel = longFilePath.GetDirectoryAtLevel(currentWorkingDirectoryLevel);

        if (StringCompare(filePathAtDirectoryLevel.m_data, currentWorkingDirectory.m_data))
        {
            isDirectoryInsideOfWorkingDirectory = true;
        }

        if (isDirectoryInsideOfWorkingDirectory)
        {
            m_stepsFromFileToCommonSharedDir = longFilePath.GetDirectoryLevel() - currentWorkingDirectoryLevel;

            const uint16 nameLengthToDirectory = StringLength(filePathAtDirectoryLevel.m_data);
            m_pathLength = longFilePath.Length() - nameLengthToDirectory;
            memcpy(m_pathFromWorkingDir, &longFilePath.m_data[nameLengthToDirectory], m_pathLength * sizeof(wchar_t));
            m_pathFromWorkingDir[m_pathLength] = g_End;
            m_isInsideWorkingDirectory = true;
            m_name = longFilePath.Object().Name().ToCharString().Data();
            return;
        }
    }

    m_isInsideWorkingDirectory = false;
    // find common lowest directory and set the data
    const int16 lowestCommonDirectoryLevel = FilePath::FindCommonLowestDirectoryLevel(longFilePath, currentWorkingDirectory);

    if (lowestCommonDirectoryLevel == -1)
    {
        m_stepsFromFileToCommonSharedDir = -1;
        m_pathLength = longFilePath.Length();
        memcpy(m_pathFromWorkingDir, &longFilePath.m_data, m_pathLength * sizeof(wchar_t));
    }
    else
    {
        m_stepsFromFileToCommonSharedDir = longFilePath.GetDirectoryLevel() - lowestCommonDirectoryLevel;
        FilePath filePathAtCommonDirectoryLevel = longFilePath.GetDirectoryAtLevel(lowestCommonDirectoryLevel);
        const uint16 nameLengthToDirectory = StringLength(filePathAtCommonDirectoryLevel.m_data);
        m_pathLength = longFilePath.Length() - nameLengthToDirectory;
        memcpy(m_pathFromWorkingDir, &longFilePath.m_data[nameLengthToDirectory], m_pathLength * sizeof(wchar_t));
        m_pathFromWorkingDir[m_pathLength] = g_End;
    }
    m_name = longFilePath.Object().Name().ToCharString().Data();
}

FilePath RelativeFilePath::GetFilePath() const
{
    if (Empty())
        return FilePath::GetCurrentWorkingDirectory();

    FilePath returnPath;

    //If not an invalid filepath or a path on a different disk from working directory
    if (m_stepsFromFileToCommonSharedDir != -1)
    {
        const uint16 currentWorkingDirectoryLevel = FilePath::GetCurrentWorkingDirectory().m_directoryLevel;
        if (m_isInsideWorkingDirectory)
        {
            //using the char buffer from the return path as a temporary storage
            memcpy(returnPath.m_data, FilePath::GetCurrentWorkingDirectory().m_data, FilePath::GetCurrentWorkingDirectory().Length() * sizeof(wchar_t));
            memcpy(&returnPath.m_data[FilePath::GetCurrentWorkingDirectory().Length()], m_pathFromWorkingDir, m_pathLength * sizeof(wchar_t));
            returnPath.m_data[FilePath::GetCurrentWorkingDirectory().Length() + m_pathLength] = g_End;
            returnPath = FilePath(returnPath.m_data);
        }
        else
        {
            const uint16 sharedDirectoryLevel = m_stepsFromFileToCommonSharedDir - Math::Abs(m_stepsFromFileToCommonSharedDir - FilePath::GetCurrentWorkingDirectory().GetDirectoryLevel());
            const FilePath sharedDirectory = FilePath::GetCurrentWorkingDirectory().GetDirectoryAtLevel(sharedDirectoryLevel);

            memcpy(returnPath.m_data, sharedDirectory.m_data, sharedDirectory.Length() * sizeof(wchar_t));
            memcpy(&returnPath.m_data[sharedDirectory.Length()], m_pathFromWorkingDir, m_pathLength * sizeof(wchar_t));
            returnPath.m_data[sharedDirectory.Length() + m_pathLength] = g_End;
            returnPath = FilePath(returnPath.m_data);
        }
    }
    else
    {
        returnPath = FilePath(m_pathFromWorkingDir);
    }

    return returnPath;
}

void RelativeFilePath::Serialize(InOutStream& outObject)
{
    if (!outObject.IsWriting())
        return; //TODO: Add assert here
    outObject.Write(&m_pathLength, sizeof(uint16));
    outObject.Write(m_pathFromWorkingDir, sizeof(wchar_t), m_pathLength);
    outObject.Write(&m_stepsFromFileToCommonSharedDir, sizeof(int16));
    outObject.Write(&m_directoryLevel, sizeof(uint16));
    outObject.Write(&m_isInsideWorkingDirectory, sizeof(bool));
}

void RelativeFilePath::Deserialize(InOutStream& inObject)
{
    if (inObject.IsWriting())
        return; //TODO: Add assert here
    inObject.Read(&m_pathLength, sizeof(uint16));
    inObject.Read(m_pathFromWorkingDir, sizeof(wchar_t), m_pathLength);
    m_pathFromWorkingDir[m_pathLength] = 0;
    inObject.Read(&m_stepsFromFileToCommonSharedDir, sizeof(int16));
    inObject.Read(&m_directoryLevel, sizeof(uint16));
    inObject.Read(&m_isInsideWorkingDirectory, sizeof(bool));
    m_name = GetFilePath().Object().Name().ToCharString().Data();
}