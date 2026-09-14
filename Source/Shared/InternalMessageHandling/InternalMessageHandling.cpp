#include "Shared_PCH.h"
#include "InternalMessageHandling.h"
#include "InternalMessageLogger.h"
#include "Hail_Time.h"
#include "DebugMacros.h"
#include "Utility/StringUtility.h"
using namespace Hail;

#ifdef DEBUG
void Hail::AssertMessage(const char* expr_str, bool expression, const char* file, int line, const char* message)
{
	do
	{
		if (!expression)
		{
			//This shit is not pretty, but good enough for now until I have a proper string class. 
			char messageString[1024];
			const uint32 exprLength = StringLength(expr_str);
			memcpy(messageString, expr_str, exprLength);
			messageString[exprLength] = ' ';
			messageString[exprLength + 1] = 0;
			AddToString(messageString, message, 1024);

			wchar_t wString[1024];
			FromConstCharToWChar(messageString, wString, 1024);
			wchar_t wFileNameString[256];
			FromConstCharToWChar(file, wFileNameString, 256);
			_wassert(wString, wFileNameString, line);
		}
	} while (0);
}
#endif

void Hail::CreateMessage(const char* message, const char* fileName, int line, eMessageType type)
{
	InternalMessage errorMessage;
	errorMessage.m_type = type;
	errorMessage.m_fileName = fileName;
	errorMessage.m_message = message;
	errorMessage.m_systemTimeLastHappened = GetGlobalTimer()->GetSystemTime();
	errorMessage.m_numberOfOccurences = 1;
	errorMessage.m_codeLine = line;
	InternalMessageLogger::GetInstance().InsertMessage(errorMessage);
	//TODO: Make a global setting or something for if console should be enabled or not.
	Debug_PrintConsoleConstChar(message);
}