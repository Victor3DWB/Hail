#include "Shared_PCH.h"
#include "FileSystem.h"
#include "DebugMacros.h"

#include <stack>
#include <string>
#include <locale> 
#include <codecvt>
#include <strsafe.h>
#include <stdarg.h>
#include "String.hpp"
#include "MathUtils.h"

#ifdef PLATFORM_WINDOWS
#include <windows.h>

#else 

#endif

namespace Hail
{
    struct FindFileData
    {
#ifdef PLATFORM_WINDOWS
        WIN32_FIND_DATA m_findFileData;
#endif
        void* m_hFind;
    };

    SelectAbleFileObject::SelectAbleFileObject(const FileObject& fileObject) : m_fileObject(fileObject)
    {
        m_selected = false;
    }

    void FileSystem::IterateOverFilesAndFillDataRecursively(const FilePath& currentPath)
    {
        FileDirectoryWithFiles currentDirectory;
        RecursiveFileIterator iterator = RecursiveFileIterator(currentPath);
        {
            const FilePath iteratedPath = iterator.GetCurrentPath();
            const FileObject baseObject = iteratedPath.Object();
            const uint16 baseDepth = baseObject.GetDirectoryLevel() - m_baseDepth;
            H_ASSERT(baseDepth == 0, "Base Depth must be 0, logic error in the code.")
            currentDirectory.directoryObject = baseObject;

            m_fileDirectories[baseDepth].Add(currentDirectory);

        }
        while (iterator.IterateOverFolderRecursively())
        {

            const FilePath iteratedPath = iterator.GetCurrentPath();
            const FileObject& currentObject = iteratedPath.Object();

            const uint16 currentDepth = iteratedPath.GetDirectoryLevel() - m_baseDepth;
            m_maxDepth = Math::Max(m_maxDepth, currentObject.GetDirectoryLevel());

            if (currentObject.IsDirectory())
            {
                if (currentObject != currentDirectory.directoryObject)
                {
                    currentDirectory.files.RemoveAll();
                    currentDirectory.directoryObject = currentObject;
                }
                m_fileDirectories[currentDepth].Add(currentDirectory);
            }

            for (size_t i = 0; i < m_fileDirectories[currentDepth - 1u].Size(); i++)
            {
                const FileObject& object = m_fileDirectories[currentDepth - 1u][i].directoryObject;
                if (object.Name() == currentObject.ParentName())
                {
                    GrowingArray<SelectAbleFileObject>& files = m_fileDirectories[currentDepth - 1u][i].files;
                    files.Add(currentObject);
                    break;
                }
            }
        }
    }

    void FileSystem::IterateOverFolderAndFillData(const FilePath& pathToUpdate)
    {
        FileIterator iterator = FileIterator(pathToUpdate);

        GrowingArray<FileDirectoryWithFiles>& directoryAtLevel = m_fileDirectories[pathToUpdate.GetDirectoryLevel() - m_baseDepth];

        while (iterator.IterateOverFolder())
        {
            const FilePath iteratedPath = iterator.GetCurrentPath();
            const FileObject& currentObject = iteratedPath.Object();

            for (size_t i = 0; i < directoryAtLevel.Size(); i++)
            {
                if (directoryAtLevel[i].directoryObject.Name() == currentObject.ParentName())
                {
                    directoryAtLevel[i].files.Add(currentObject);
                    break;
                }
            }
        }
    }

    void FileSystem::JumpUpOneDirectory(const SelectAbleFileObject& directoryToJumpTo)
    {
        if (directoryToJumpTo.m_fileObject.IsDirectory())
        {
            WString64 name = directoryToJumpTo.m_fileObject.Name();
            if (name.Data()[name.Length() - 1] != g_SourceSeparator)
            {
                name = name + g_SourceSeparatorAndEnd;
            }
            FilePath newPath = m_basePath + name;
            if (newPath.IsValid())
            {
                m_basePath = newPath;
                m_currentDepth = m_basePath.GetDirectoryLevel();
                m_currentFileDirectoryObject = directoryToJumpTo.m_fileObject;
                IterateOverFolder(m_basePath);
                SetDirectories(m_basePath);
            }
        }
    }

    void FileSystem::JumpToParent()
    {
        m_basePath = m_basePath.Parent();

        m_currentDepth = m_basePath.GetDirectoryLevel();
        m_directories[m_currentDepth - 1] = m_basePath.Object();
        IterateOverFolder(m_basePath);
        SetDirectories(m_basePath);
    }

    void FileSystem::JumpToDepth(uint16_t depthToGoTo)
    {
        if (depthToGoTo <= m_maxDepth && depthToGoTo != m_basePath.GetDirectoryLevel())
        {
            FilePath newPath = m_basePath;
            if (depthToGoTo < m_basePath.GetDirectoryLevel())
            {
                while (newPath.GetDirectoryLevel() != depthToGoTo)
                {
                    newPath = newPath.Parent();
                }
            }
            else
            {
                for (uint16_t i = m_basePath.GetDirectoryLevel(); i < depthToGoTo; i++)
                {
                    WString64 name = m_directories[i + 1].m_fileObject.Name();
                    if (name.Data()[name.Length() - 1] != g_SourceSeparator)
                    {
                        name = name + g_SourceSeparatorAndEnd;
                    }
                    newPath = newPath + m_directories[i + 1].m_fileObject.Name();
                }
            }
            if (newPath.IsValid())
            {
                m_basePath = newPath;
                m_currentFileDirectoryObject = m_basePath.Object();
                m_currentDepth = m_basePath.GetDirectoryLevel();
                IterateOverFolder(m_basePath);
                SetDirectories(m_basePath);
            }
        }
    }

    void FileSystem::ReloadFolder(const FilePath& folderToReload)
    {
        if (!folderToReload.IsValid())
            return;

        const uint16 folderLevel = folderToReload.IsDirectory() ? folderToReload.GetDirectoryLevel() : folderToReload.GetDirectoryLevel() - 1;
        if (!m_isInitialized || folderLevel < m_baseDepth || folderLevel > m_maxDepth)
            return;

        const FileObject directoryObject = folderToReload.IsDirectory() ? folderToReload.Object() : folderToReload.Parent().Object();

        GrowingArray<FileDirectoryWithFiles>& directoryAtLevel = m_fileDirectories[folderLevel - m_baseDepth];
        bool folderAlreadyExists = false;
        uint16 indexOfFolder = 0;
        for (size_t i = 0; i < directoryAtLevel.Size(); i++)
        {
            const FileObject directoryObjectAtLevel = directoryAtLevel[i].directoryObject;
            if (directoryObjectAtLevel.Name() == directoryObject.Name())
            {
                folderAlreadyExists = true;
                indexOfFolder = i;
                break;
            }
        }

        if (folderAlreadyExists)
        {
            directoryAtLevel[indexOfFolder].files.RemoveAll();
            directoryAtLevel[indexOfFolder].directoryObject = directoryObject; //updating the fileData of the object to latest
            IterateOverFolderAndFillData(folderToReload.IsDirectory() ? folderToReload : folderToReload.Parent());
            return;
        }
        FileDirectoryWithFiles directory;
        directory.directoryObject = directoryObject;
        directoryAtLevel.Add(directory);
        IterateOverFolderAndFillData(folderToReload.IsDirectory() ? folderToReload : folderToReload.Parent());
    }


    const GrowingArray<FileDirectoryWithFiles>* FileSystem::GetFileDirectoriesAtDepth(uint16 directoryDepth)
    {
        if (!m_isInitialized || directoryDepth < m_baseDepth || directoryDepth > m_maxDepth)
            return nullptr;
        return &m_fileDirectories[directoryDepth - m_baseDepth];
    }

    GrowingArray<SelectAbleFileObject>* FileSystem::GetCurrentFileDirectory()
    {
        if (!m_isInitialized || m_currentDepth < m_baseDepth)
            return nullptr;

        for (size_t i = 0; i < m_fileDirectories[m_currentDepth - m_baseDepth].Size(); i++)
        {
            if (m_fileDirectories[m_currentDepth - m_baseDepth][i].directoryObject.Name() == m_currentFileDirectoryObject.Name())
            {
                return &m_fileDirectories[m_currentDepth - m_baseDepth][i].files;
            }
        }
        return nullptr;
    }

    const GrowingArray<SelectAbleFileObject>* FileSystem::GetFileDirectory(const FileObject& fileDirectory)
    {
        if (!m_isInitialized)
            return nullptr;
        if (fileDirectory.GetDirectoryLevel() < m_baseDepth)
            return nullptr;
        for (size_t i = 0; i < m_fileDirectories[fileDirectory.GetDirectoryLevel() - m_baseDepth].Size(); i++)
        {
            if (m_fileDirectories[fileDirectory.GetDirectoryLevel() - m_baseDepth][i].directoryObject.Name() == fileDirectory.Name())
            {
                return &m_fileDirectories[fileDirectory.GetDirectoryLevel() - m_baseDepth][i].files;
            }
        }
        return nullptr;
    }

    bool FileSystem::SetCurrentFileDirectory(const FileObject& directory)
    {
        int16 directoryLevel;
        if (directory.IsDirectory())
        {
            directoryLevel = directory.GetDirectoryLevel() - m_baseDepth;
        }
        else
        {
            if (directory.GetDirectoryLevel() == 0)
                return false;
            directoryLevel = (directory.GetDirectoryLevel() - m_baseDepth) - 1;
        }
        bool foundDirectory = false;
        if (directoryLevel >= 0)
        {
            for (size_t i = 0; i < m_fileDirectories[directoryLevel].Size(); i++)
            {
                if (m_fileDirectories[directoryLevel][i].directoryObject.Name() == directory.Name())
                {
                    m_currentFileDirectoryObject = directory;
                    m_currentDepth = directoryLevel + m_baseDepth;
                    foundDirectory = true;
                    break;
                }
            }
        }
        else
        {
            if (m_directories[directory.GetDirectoryLevel()].m_fileObject.Name() == directory.Name())
            {
                m_currentFileDirectoryObject = directory;
                m_currentDepth = directoryLevel + m_baseDepth;
                foundDirectory = true;
            }
        }

        if (foundDirectory)
        {
            if (m_basePath.Object() != directory)
            {
                m_basePath = FilePath();
                for (size_t i = 0; i < directory.GetDirectoryLevel(); i++)
                {
                    m_basePath = m_basePath + m_directories[i].m_fileObject;
                }
                m_basePath = m_basePath + directory;
            }
            IterateOverFolder(m_basePath);
            SetDirectories(m_basePath);
        }

        return foundDirectory;
    }

    void FileSystem::IterateOverFolder(const FilePath& currentPath)
    {
        if (currentPath.IsValid())
        {
            m_currentDirectoryFiles.RemoveAll();
            FileIterator iterator = FileIterator(currentPath);
            while (iterator.IterateOverFolder())
            {
                const FilePath iteratedPath = iterator.GetCurrentPath();
                const FileObject currentObject = iteratedPath.Object();
                if (currentObject.IsDirectory())
                {
                    m_currentDirectoryFiles.Add(currentObject);
                }
                else
                {
                    if (m_extensionsToSearchFor.Empty())
                    {
                        m_currentDirectoryFiles.Add(currentObject);
                    }
                    else
                    {
                        bool isObjectInList = false;
                        for (uint32_t i = 0; i < m_extensionsToSearchFor.Size(); i++)
                        {
                            if (m_extensionsToSearchFor[i] == currentObject.Extension().ToCharString())
                            {
                                isObjectInList = true;
                                break;
                            }
                        }
                        if (isObjectInList)
                        {
                            m_currentDirectoryFiles.Add(currentObject);
                        }
                    }
                }
            }
        }
    }

    void FileSystem::SetDirectories(FilePath path)
    {
        m_maxDepth = Math::Max(m_maxDepth, path.GetDirectoryLevel());
        while (path.GetDirectoryLevel() != 0)
        {
            m_directories[path.GetDirectoryLevel()] = path.Object();
            if (path.GetDirectoryLevel() == 1)
            {
                break;
            }
            path = path.Parent();
        }
        m_directories[0] = path.Parent().Object();
    }

    void FileSystem::Initialize()
    {
        m_isInitialized = true;
    }

    void FileSystem::Reset()
    {
        m_basePath = {};
        m_isInitialized = {};
        for (size_t i = 0; i < MAX_FILE_DEPTH; i++)
        {
            m_fileDirectories[i].DeleteAll();
            m_directories[i] = SelectAbleFileObject();
        }
        m_currentDirectoryFiles.DeleteAll();
        m_currentFileDirectoryObject = FileObject();
        m_currentDepth = {};
        m_basePath = {};
        m_extensionsToSearchFor.Clear();
        m_baseDepth = {};
        m_maxDepth = {};
    }

    bool FileSystem::SetFilePath(const FilePath& basePath)
    {
        m_extensionsToSearchFor.Clear();
        m_baseDepth = basePath.GetDirectoryLevel();
        if (basePath.IsValid() && basePath.IsDirectory())
        {
            Initialize();
            m_basePath = basePath;
            SetDirectories(m_basePath);
            m_currentDepth = m_basePath.GetDirectoryLevel();
            IterateOverFolder(m_basePath);

            IterateOverFilesAndFillDataRecursively(m_basePath);
            if (!SetCurrentFileDirectory(m_basePath.Object()))
            {
                // TODO make a cleanup function
                return false;
            }
            return true;
        }
        m_baseDepth = 0;
        m_isInitialized = false;
        return false;
    }

    bool FileSystem::SetFilePathAndInit(const FilePath& basePath, const VectorOnStack<String64, 8>& extensionsToSearchFor)
    {
        if (SetFilePath(basePath))
        {
            m_extensionsToSearchFor = extensionsToSearchFor;
            return true;
        }
        return false;
    }

    GrowingArray<SelectAbleFileObject>& FileSystem::GetFilesAtCurrentDepth()
    {
        return m_currentDirectoryFiles;
    }

    const SelectAbleFileObject& FileSystem::GetDirectoryAtDepth(uint16_t requestedDepth)
    {
        return m_directories[requestedDepth];
    }

    const SelectAbleFileObject& FileSystem::GetDirectoryAtCurrentDepth()
    {
        return GetDirectoryAtDepth(m_currentDepth);
    }

    FileIterator::FileIterator()
    {
        Init();
    }

    FileIterator::~FileIterator()
    {
        DeInit();
    }

    FileIterator::FileIterator(const FilePath& basePath)
    {
        Init();
        m_basePath = basePath;
        m_baseDepth = m_basePath.GetDirectoryLevel();
        InitPath(m_basePath);
        if (m_osHandleIsOpen)
            m_currentFileObject = basePath.Object();
    }

    void FileIterator::Init()
    {
        //TODO: make sure that this object is in pooled memory to prevent fragmentation
        m_currentFileFindData = new FindFileData();
    }

    void FileIterator::DeInit()
    {
        if (m_osHandleIsOpen)
        {
            FindClose(((FindFileData*)m_currentFileFindData)->m_hFind);
        }
        delete m_currentFileFindData;
        m_currentFileFindData = nullptr;
    }

    void FileIterator::InitPath(const FilePath& basePath)
    {
        m_currentFileObject = FileObject();
        FilePath currentPath = basePath;
        currentPath.AddWildcard();

        if (currentPath.IsValid())
        {
#ifdef PLATFORM_WINDOWS
            WIN32_FIND_DATA findFileData;
            HANDLE hFind = FindFirstFile(currentPath.Data(), &findFileData);
            m_osHandleIsOpen = hFind != 0;
            ((FindFileData*)m_currentFileFindData)->m_findFileData = findFileData;
            ((FindFileData*)m_currentFileFindData)->m_hFind = hFind;
#else 

#endif
        }
        else
        {
            m_osHandleIsOpen = false;
        }
    }

    bool FileIterator::IterateOverFolder()
    {
        if (!m_osHandleIsOpen)
            return false;

        FilePath currentPath = m_basePath + m_currentFileObject;
        currentPath.AddWildcard();
        FindFileData& currentFileData = *((FindFileData*)m_currentFileFindData);
        do 
        {
            if ((currentFileData.m_findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0
                && wcscmp(currentFileData.m_findFileData.cFileName, L".") != 0
                && wcscmp(currentFileData.m_findFileData.cFileName, L"..") != 0)
            {
                m_currentFileObject = FileObject(currentFileData.m_findFileData.cFileName, m_basePath.Object(), ConstructFileData(&currentFileData.m_findFileData), true);
                if (FindNextFile(currentFileData.m_hFind, &currentFileData.m_findFileData) == 0)
                {
                    m_osHandleIsOpen = false;
                    FindClose(currentFileData.m_hFind);
                }
                return true;
            }
            else
            {
                if (wcscmp(currentFileData.m_findFileData.cFileName, L".") != 0
                    && wcscmp(currentFileData.m_findFileData.cFileName, L"..") != 0)
                {
                    m_currentFileObject = FileObject(currentFileData.m_findFileData.cFileName, m_basePath.Object(), ConstructFileData(&currentFileData.m_findFileData), false);
                    if (FindNextFile(currentFileData.m_hFind, &currentFileData.m_findFileData) == 0)
                    {
                        m_osHandleIsOpen = false;
                        FindClose(currentFileData.m_hFind);
                    }
                    return true;
                }
            }
        } while (FindNextFile(currentFileData.m_hFind, &currentFileData.m_findFileData) != 0);
        m_osHandleIsOpen = false;
        FindClose(currentFileData.m_hFind);
        return true;
    }

    FilePath FileIterator::GetCurrentPath() const
    {
        FilePath returnPath = m_basePath;
        return returnPath + m_currentFileObject;
    }

    RecursiveFileIterator::RecursiveFileIterator(const FilePath& basePath) : FileIterator(basePath)
    {
    }

    bool RecursiveFileIterator::IterateOverFolderRecursively()
    {
        const bool iterationResult = IterateOverFolder();
        if (iterationResult)
        {
            if (m_currentFileObject.IsDirectory())
            {
                m_directoriesToIterateOver.Push(m_basePath + m_currentFileObject);
            }
        }
        else
        {
            if (m_directoriesToIterateOver.Size() > 0)
            {
                m_basePath = m_directoriesToIterateOver.Pop();
                InitPath(m_basePath);
                IterateOverFolderRecursively();
            }
            else
            {
                return false;
            }
        }
        return true;
    }



}

