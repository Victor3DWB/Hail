#include "Shared_PCH.h"
#include "ErrorHandler.h"
#include "DebugMacros.h"
#include "Utility/StringUtility.h"
#include "Utility/InOutStream.h"
#include "Timer.h"

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#endif

using namespace Hail;

String64 LocalGetStartErrorString(EStartupErrors startupErrror)
{
	switch (startupErrror)
	{
	case Hail::InitResourceRegistry:
		return "\tResource Registry Error.\n";
		break;
	case Hail::InitDebugRenderer:
		return "\tInitialize Debug Rendering Error.\n";
			break;
	case Hail::InitCloudRenderer:
		return "\tInitialize Cloud Rendering Error.\n";
		break;
	case Hail::InitFontRenderer:
		return "\tInitialize Font Rendering Error.\n";
		break;
	case Hail::InitTextureManager:
		return "\tInitialize Texture Manager Error.\n";
		break;
	case Hail::InitMaterialManager:
		return "\tInitialize Material Manager Error.\n";
		break;
	case Hail::InitResourceManager:
		return "\tInitialize Resource Manager Error.\n";
		break;
	case Hail::InitDefaultMaterial:
		return "\tFailed to create default materials.\n";
		break;
	case Hail::InitRenderingResourceManager:
		return "\tInitialize Rendering Resource Manager Error.\n";
		break;
	case Hail::InitSwapChain:
		return "\tInitialize Swapchain Error.\n";
		break;
	case Hail::InitGpuDevice:
		return "\tInitialize Device Error.\n";
		break;
	default:
		break;
	}
}

Hail::ErrorManager::ErrorManager()
{
	memset(m_errors.Data(), 0, sizeof(uint64) * (uint32)EErrorType::Count);
}

void Hail::ErrorManager::AddErrors(uint64 errors, EErrorType errorType)
{
	H_ASSERT(errorType != EErrorType::Count);
	m_errors[(uint32)errorType] |= errors;
}

void Hail::ErrorManager::DumpErrorLog() const
{
	StringL errorString;

	errorString += StringL::Format("Device information:\n\t%s\n", m_errorString.Data());

	errorString += "Logged errors:\n";
	const StringL formattedErrorCode = StringL::Format("Startup Error code: %u\n", m_errors[(uint32)EErrorType::Startup]);
	errorString += formattedErrorCode;
	for (uint32 i = 0; i < 64u; i++)
	{
		const uint64 mask = 1u << i;
		for (uint32 iErrorType = 0; iErrorType < (uint32)EErrorType::Count; iErrorType++)
		{
			if (m_errors[(uint32)iErrorType] & mask)
			{
				const EErrorType errorType = (EErrorType)iErrorType;
				switch (errorType)
				{
				case Hail::EErrorType::Startup:
					errorString += LocalGetStartErrorString((EStartupErrors)mask);
					break;
				case Hail::EErrorType::Count:
					break;
				default:
					break;
				}
			}
		}
	}

	errorString += "End of error codes.";

	Timer timer;
	const uint64 timeInNanoSeconds = timer.GetSystemTime();

	String64 timeString;
#ifdef PLATFORM_WINDOWS
	FILETIME time;
	time.dwLowDateTime = (uint32)timeInNanoSeconds;
	time.dwHighDateTime = timeInNanoSeconds >> 32;
	SYSTEMTIME SystemTime{};
	FileTimeToSystemTime(&time, &SystemTime);
	timeString = String64::Format("%u%u%u-%u%u%u", SystemTime.wYear, SystemTime.wMonth, SystemTime.wDay, SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond);
#endif
	WString64 timeStringW;
	FromConstCharToWChar(timeString, timeStringW, 64u);
	const FilePath& userDirectory = FilePath::GetUserProjectDirectory();
	const StringLW dmpFileName = StringLW::Format(L"%s%s%s-%s-%d-%d.txt", userDirectory.Data(), L"SystemLog", L"V1.0", timeStringW.Data(), GetCurrentProcessId(), GetCurrentThreadId());

	Hail::InOutStream inOutStream;
	inOutStream.OpenFile(dmpFileName.Data(), Hail::FILE_OPEN_TYPE::WRITE, false);
	inOutStream.Write(errorString.Data(), sizeof(char), errorString.Length());
	inOutStream.CloseFile();

}

void Hail::ErrorManager::AddString(StringL error)
{
	if (m_errorString.Length() == 0)
	{
		m_errorString += "..... \n";
	}

	m_errorString += error;
	m_errorString += '\n';
}

bool Hail::ErrorManager::GetAreErrorsLogged() const
{
	bool bAreErrorsLogged = false;
	for (uint32 i = 0; i < (uint32)EErrorType::Count; i++)
	{
		bAreErrorsLogged |= (m_errors[i] != 0);
	}
	return bAreErrorsLogged;
}
