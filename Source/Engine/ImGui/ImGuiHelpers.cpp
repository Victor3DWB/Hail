#include "Engine_PCH.h"
#include "ImGuiHelpers.h"


#include "imgui.h"
#include "Utility\FileSystem.h"
#include "Utility\StringUtility.h"
#include "ImGuiHelpers.h"
#include "MetaResource.h"

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#endif

using namespace Hail;

bool ImGuiHelpers::DirectoryPanelLogic(FileSystem* fileSystem, uint32 minimumDepth)
{
    uint16_t depthWeReached = 0;
    uint16_t selectedDepth = fileSystem->GetCurrentDepth();
    bool jumpToADepth = false;
    StaticArray<FileObject, MAX_FILE_DEPTH> fileObjects;
    FilePath filePath = fileSystem->GetCurrentFilePath();

    for (size_t i = 0; i < fileSystem->GetCurrentDepth() + 1; i++)
    {
        fileObjects[filePath.GetDirectoryLevel()] = filePath.Object();
        filePath = filePath.Parent();
    }

    FileObject selectedFileObject = fileSystem->GetCurrentFileDirectoryObject();

    for (size_t i = minimumDepth; i < fileSystem->GetCurrentDepth() + 1; i++)
    {
        const FileObject& directoryObject = fileObjects[i];
        StringL folderName = directoryObject.Name().ToCharString();
        if (i == fileSystem->GetCurrentDepth())
        {
            folderName = StringL("-> ") + folderName;
        }
        if (ImGui::Selectable(folderName.Data(), false) && i != fileSystem->GetCurrentDepth())
        {
            if (selectedDepth != directoryObject.GetDirectoryLevel())
                jumpToADepth = true;
            selectedDepth = directoryObject.GetDirectoryLevel();
            selectedFileObject = directoryObject;
        }
        depthWeReached++;
        ImGui::Indent();
        if (i == fileSystem->GetCurrentDepth())
        {
            GrowingArray<SelectAbleFileObject>& filesAtDepth = fileSystem->GetFilesAtCurrentDepth();

            for (size_t iFileObject = 0; iFileObject < filesAtDepth.Size(); iFileObject++)
            {
                const SelectAbleFileObject& fileObject = filesAtDepth[iFileObject];
                if (!fileObject.m_fileObject.IsDirectory())
                    continue;
                if (ImGui::Selectable(fileObject.m_fileObject.Name().ToCharString().Data(), fileObject.m_selected))
                {
                    selectedFileObject = fileObject.m_fileObject;
                    selectedDepth = fileObject.m_fileObject.GetDirectoryLevel();
                }
            }
        }
    }

    for (uint32_t i = 0; i < depthWeReached; i++)
    {
        ImGui::Unindent();
    }

    bool fileSystemUpdated = false;
    if (selectedFileObject != fileSystem->GetCurrentFileDirectoryObject() && selectedFileObject.GetDirectoryLevel() >= minimumDepth)
    {
        if (!fileSystem->SetCurrentFileDirectory(selectedFileObject))
        {
            fileSystem->JumpToDepth(selectedDepth);
        }
        fileSystemUpdated = true;
    }
    if (jumpToADepth)
    {
        fileSystem->JumpToDepth(selectedDepth);
        fileSystemUpdated = true;
    }
    return fileSystemUpdated;
}

void Hail::ImGuiHelpers::MetaResourcePanel(const MetaResource* pMetaResource)
{
    if (!pMetaResource)
        return;

    if (pMetaResource->GetGUID() == GUID())
    {
        ImGui::Text("No meta resource associated with this resource, reload / reimport the resource.");
    }
    else
    {
        const GUID& metaID = pMetaResource->GetGUID();
        char pathString[MAX_FILE_LENGTH];
        FromWCharToConstChar(pMetaResource->GetProjectFilePath().GetRelativePathData(), pathString, MAX_FILE_LENGTH);
        ImGui::Text("Project Path : ..%s", pathString);
        ImGui::Text("GUID: %xll %xh %xh %c%c%c%c%c%c%c%c", metaID.m_data1, metaID.m_data2, metaID.m_data3,
            metaID.m_data4[0], metaID.m_data4[1], metaID.m_data4[2], metaID.m_data4[3], 
            metaID.m_data4[4], metaID.m_data4[5], metaID.m_data4[6], metaID.m_data4[7]);
        ImGui::Separator();
        ImGui::Text("Base resource information");
        FromWCharToConstChar(pMetaResource->GetSourceFilePath().GetRelativePathData(), pathString, MAX_FILE_LENGTH);
        ImGui::Text("Source Path : ..%s", pathString);
        ImGui::Text("Creation Time: %s", FormattedTimeFromFileData(pMetaResource->GetSourceFileData().m_creationTime));
        ImGui::Text("Last Edited Time: %s", FormattedTimeFromFileData(pMetaResource->GetSourceFileData().m_lastWriteTime));
        ImGui::Text("File size: %f mb", (float)((double)pMetaResource->GetSourceFileData().m_filesizeInBytes) / 1000000.f);
    }
    //static float arr[] = { 0.6f, 0.1f, 1.0f, 0.5f, 0.92f, 0.1f, 0.2f };
    //ImGui::PlotLines("Curve", arr, IM_ARRAYSIZE(arr));
}

void Hail::ImGuiHelpers::MetaResourceTooltipPanel(const MetaResource* pMetaResource)
{
    if (!pMetaResource)
        return;
    ImGui::BeginTooltip();
    MetaResourcePanel(pMetaResource);
    ImGui::EndTooltip();
}

void Hail::ImGuiHelpers::TextWithHoverHint(const char* textToShow)
{
    ImGui::Text(textToShow);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text(textToShow);
        ImGui::EndTooltip();
    }
}

void Hail::ImGuiHelpers::ColoredTextWithHoverHint(glm::vec4 color, const char* textToShow)
{
    ImGui::TextColored(ImVec4(color.x, color.y, color.z, color.w), textToShow);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text(textToShow);
        ImGui::EndTooltip();
    }
}

String64 Hail::ImGuiHelpers::FormattedTimeFromFileData(const FileTime& fileTime)
{
    String64 timePreview;
#ifdef PLATFORM_WINDOWS
    FILETIME time;
    time.dwLowDateTime = fileTime.m_lowDateTime;
    time.dwHighDateTime = fileTime.m_highDateTime;
    SYSTEMTIME SystemTime{};
    FileTimeToSystemTime(&time, &SystemTime);
    timePreview = String64::Format("%u/%u/%u  %u:%u:%u", SystemTime.wYear, SystemTime.wMonth, SystemTime.wDay, SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond);
#endif
    return timePreview;
}

const char* Hail::ImGuiHelpers::GetMaterialTypeStringFromEnum(eMaterialType type)
{
    switch (type)
    {
    case Hail::eMaterialType::SPRITE:
        return "Sprite";
    case Hail::eMaterialType::MODEL3D:
        return "3D Model";
    case Hail::eMaterialType::FULLSCREEN_PRESENT_LETTERBOX:
    case Hail::eMaterialType::COUNT:
    default:
        break;
    }
    return nullptr;
}

const char* Hail::ImGuiHelpers::GetMaterialBlendModeFromEnum(eBlendMode mode)
{
    switch (mode)
    {
    case Hail::eBlendMode::None:
        return "None";
    case Hail::eBlendMode::Cutout:
        return "Cut out";
    case Hail::eBlendMode::Translucent:
        return "Translucent";
    case Hail::eBlendMode::Additive:
        return "Additive";
    case Hail::eBlendMode::COUNT:
    default:
        break;
    }
    return nullptr;
}

const char* Hail::ImGuiHelpers::GetShaderTypeFromEnum(eShaderStage mode)
{
    switch (mode)
    {
    case Hail::eShaderStage::Compute:
        return "Compute";
    case Hail::eShaderStage::Vertex:
        return "Vertex";
    case Hail::eShaderStage::Fragment:
        return "Fragment";
    case Hail::eShaderStage::Amplification:
        return "Amplification";
    case Hail::eShaderStage::Mesh:
        return "Mesh";
    case Hail::eShaderStage::None:
        return "None";
    default:
        break;
    }

    return nullptr;
}

int Hail::ImGuiHelpers::GetMaterialTypeComboBox(uint32 index)
{
    int materialIndex = index;
    if (ImGui::BeginCombo("Material Base Type", ImGuiHelpers::GetMaterialTypeStringFromEnum((eMaterialType)index)))
    {
        bool is_selected = (eMaterialType::SPRITE == (eMaterialType)index);
        if (ImGui::Selectable(ImGuiHelpers::GetMaterialTypeStringFromEnum(eMaterialType::SPRITE), is_selected))
        {
            materialIndex = (int)eMaterialType::SPRITE;
        }
        if (is_selected)
            ImGui::SetItemDefaultFocus();
        //
        is_selected = (eMaterialType::MODEL3D == (eMaterialType)index);
        if (ImGui::Selectable(ImGuiHelpers::GetMaterialTypeStringFromEnum(eMaterialType::MODEL3D), is_selected))
        {
            materialIndex = (int)eMaterialType::MODEL3D;
        }
        if (is_selected)
            ImGui::SetItemDefaultFocus();

        ImGui::EndCombo();
    }
    return materialIndex;
}
