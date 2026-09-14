#include "Game_PCH.h"
#include "ErrorHandler.h"
#include "Debugger.h"
#include "Runner.h"

#include "angelscript.h"

#include "Utility/InOutStream.h"

using namespace Hail;

void AngelScript::ErrorHandler::Init(asIScriptEngine* pScriptEngine, bool bEnableDebugger)
{
	m_pRunner = nullptr;
	// Set the message callback to receive information on errors in human readable form.
	int r = pScriptEngine->SetMessageCallback(asMETHOD(ErrorHandler, MessageCallback), this, asCALL_THISCALL);
	H_ASSERT(r >= 0, "Failed to set AngelScript message callback");
	m_bEnableDebugger = bEnableDebugger;
}

void AngelScript::ErrorHandler::MessageCallback(const asSMessageInfo* msg, void* param)
{
	H_ASSERT(m_bEnableDebugger);
	StringL message;

	if (msg->type == asMSGTYPE_ERROR && m_pRunner && m_pRunner->GetDebuggerServer())
	{
		BuildErrorInfo buildError;

		buildError.m_col = msg->col;
		buildError.m_row = msg->row;
		buildError.m_section = msg->section;
		buildError.m_message = msg->message;

		if (StringCompare("Expected ';'", msg->message) || StringCompare("Expected ',' or ';'", msg->message))
		{
			const FilePath scriptFilePath = msg->section;
			InOutStream inStream;

			const bool doesCompiledShaderExist = inStream.OpenFile(scriptFilePath, FILE_OPEN_TYPE::READ, false);
			H_ASSERT(doesCompiledShaderExist);
			
			if (doesCompiledShaderExist)
			{
				StringL fileData;
				fileData.Reserve(inStream.GetFileSize());
				inStream.Read(fileData.Data(), 1u, inStream.GetFileSize());
				inStream.CloseFile();

				uint32 lastLineWithContent = 1u;
				uint32 lastColWithContent = 0u;
				uint32 lineCounter = 0u;
				uint32 lastColChar = 1u;
				bool bLineContainedNonSpaceCharacter = false;
				bool bLineIsComment = false;
				bool bIsInMultiSpaceComment = false;

				for (uint32 iChar = 0; iChar < fileData.Length(); iChar++)
				{
					const uint8 currentChar = fileData[iChar];
					if (currentChar == '\n')
					{
						if (lineCounter + 1 == buildError.m_row)
						{
							break;
						}
						lineCounter++;
						if (lastColChar > 0)
						{
							if (bLineContainedNonSpaceCharacter)
							{
								lastLineWithContent = lineCounter;
								lastColWithContent = lastColChar;
							}
						}
						lastColChar = 0u;
						bLineContainedNonSpaceCharacter = false;
						bLineIsComment = false;
					}
					else
					{
						if (currentChar != ' ')
						{
							const bool bNextCharComboIsComment = currentChar == '/' && fileData[iChar + 1] == '/';
							const bool bNextCharComboIsMultiCommentBegin = currentChar == '/' && fileData[iChar + 1] == '*';

							if (!bLineContainedNonSpaceCharacter && bNextCharComboIsComment || bNextCharComboIsMultiCommentBegin || bIsInMultiSpaceComment)
							{
								if (bNextCharComboIsMultiCommentBegin)
								{
									bIsInMultiSpaceComment = true;
								}
								bLineIsComment = true;
							}

							if (!bLineIsComment)
							{
								bLineContainedNonSpaceCharacter = true;
							}

							bool bNextCharComboIsMultiCommentEnd = false;
							if (lastColChar != 0 && currentChar == '/' && fileData[iChar - 1] == '*')
							{
								bNextCharComboIsMultiCommentEnd = true;
							}
							if (bIsInMultiSpaceComment && bNextCharComboIsMultiCommentEnd)
							{
								bIsInMultiSpaceComment = false;
							}
						}

						lastColChar++;
					}
				}
				buildError.m_col = lastColWithContent;
				H_ASSERT(lastLineWithContent != 0u);
				buildError.m_row = lastLineWithContent;
			}
		}
		message = StringL::Format("%s (%d, %d) : %s\n", msg->section, buildError.m_row, buildError.m_col, msg->message);
		buildError.m_row -= 1;
		m_pRunner->GetDebuggerServer()->AddBuildError(buildError);
	}
	else
	{
		message = StringL::Format("%s (%d, %d) : %s\n", msg->section, msg->row, msg->col, msg->message);
	}

	if (msg->type == asMSGTYPE_WARNING)
	{
		H_WARNING(message);
	}
	else if (msg->type == asMSGTYPE_INFORMATION)
	{
		H_DEBUGMESSAGE(message);
	}
	else if (msg->type == asMSGTYPE_ERROR)
	{
		H_ERROR(message);
	}
}


void Hail::AngelScript::ErrorHandler::SetActiveScriptRunner(AngelScript::Runner* pRunner)
{
	m_pRunner = pRunner;
}

