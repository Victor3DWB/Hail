#include "Engine_PCH.h"
#include "DebuggerMessagePackager.h"

#include "Debugger.h"
#include "TypeRegistry.h"
#include "Utility\FilePath.hpp"


using namespace Hail;
using namespace AngelScript;


namespace ReadMessages
{

	void ReadAndIncrementReadPoint(void* destination, void* streamToReadFrom, uint32& currentReadPoint, uint32 sizeToRead)
	{
		memcpy(destination, (uint8*)streamToReadFrom + currentReadPoint, sizeToRead);
		currentReadPoint += sizeToRead;
	}

	void ReadAndIncrementString(StringL& destination, void* streamToReadFrom, uint32& currentReadPoint)
	{
		int32 stringLength;
		ReadAndIncrementReadPoint(&stringLength, streamToReadFrom, currentReadPoint, 4);
		destination.Reserve(stringLength);
		ReadAndIncrementReadPoint(destination, streamToReadFrom, currentReadPoint, stringLength);
	}

	bool IsPathAValidProjectPath(const FilePath& filePathToValidate)
	{
		const int16 commonLowestDirectoryLevel = FilePath::FindCommonLowestDirectoryLevel(filePathToValidate, FilePath::GetAngelscriptDirectory());
		return commonLowestDirectoryLevel == FilePath::GetAngelscriptDirectory().GetDirectoryLevel();
	}

	void GenerateFileBreakPoints(DebuggerServer* pDebugger, MessageHeader header, void* messageStream)
	{
		uint32 currentReadPoint = 0;
		FileBreakPoints breakPoints;

		ReadAndIncrementString(breakPoints.fileName, messageStream, currentReadPoint);

		const FilePath filePath = breakPoints.fileName;


		if (!IsPathAValidProjectPath(filePath))
		{
			H_ERROR("Recieving breakpoints from a non project path.")
			return;
		}
		else
		{
			breakPoints.fileName = filePath.Object().Name().ToCharString();
			// Get filename
		}

		int16 numberOfBreakPoints;
		ReadAndIncrementReadPoint(&numberOfBreakPoints, messageStream, currentReadPoint, 2);
		for (size_t i = 0; i < numberOfBreakPoints; i++)
		{
			BreakPoint breakPoint;
			ReadAndIncrementReadPoint(&breakPoint.line, messageStream, currentReadPoint, 2);
			ReadAndIncrementReadPoint(&breakPoint.bHasConditional, messageStream, currentReadPoint, 1);
			if (breakPoint.bHasConditional)
			{
				ReadAndIncrementString(breakPoint.condition, messageStream, currentReadPoint);
			}
			breakPoints.breakPoints.Add(breakPoint);
		}

		pDebugger->AddBreakpoints(breakPoints);
	}

	void DecodeVariableRequest(DebuggerServer* pDebugger, MessageHeader header, void* messageStream)
	{
		StringL request;
		uint32 readPoint = 0;
		ReadAndIncrementString(request, messageStream, readPoint);

		int32 separatorSymbolIndex = StringUtility::FindFirstOfSymbol(request, ':');
		if (separatorSymbolIndex == -1)
		{
			H_ERROR("Invalid variable request, no separator in message. Check debugger extension.");
			// TODO: Send empty return message
		}
		int32 frameReference = StringUtility::IntFromConstChar(request, 0);
		const char* stackType = request.Data() + separatorSymbolIndex + 1;


		if (StringCompare(stackType, "local"))
		{
			pDebugger->SendVariables(eCallStack::local);
		}
		else if (StringCompare(stackType, "this"))
		{
			pDebugger->SendVariables(eCallStack::self);
		}
		else if (StringCompare(stackType, "global"))
		{
			pDebugger->SendVariables(eCallStack::global);
		}

		// TODO: fix for child fetching
		if (StringContains(stackType, '.'))
		{

		}
	}

	void DecodeEvaluateRequest(DebuggerServer* pDebugger, MessageHeader msgHeader, void* messageStream)
	{
		uint32 readPoint = 0;
		StringL requestExpression;
		ReadAndIncrementString(requestExpression, messageStream, readPoint);
		uint32 scope = 0;
		ReadAndIncrementReadPoint(&scope, messageStream, readPoint, sizeof(uint32));
		pDebugger->FindVariable((eCallStack)scope, requestExpression);
	}

	void HandleDebuggerMessageInternal(DebuggerServer* pDebugger, MessageHeader header, void* messageStream)
	{
		switch (header.type)
		{
		case eDebuggerMessageType::StartDebugSession:
			pDebugger->StartDebugging();
			H_DEBUGMESSAGE("Recieved start session.");
			break;
		case eDebuggerMessageType::Paused:
			pDebugger->PauseDebugging();
			H_DEBUGMESSAGE("Recieved pause.");
			break;
		case eDebuggerMessageType::Disconnect:
			pDebugger->StopDebugging();
			H_DEBUGMESSAGE("Recieved disconnect.");
			break;
		case eDebuggerMessageType::Continued:
			pDebugger->ContinueDebugging();
			H_DEBUGMESSAGE("Recieved Continued.");
			break;
		case eDebuggerMessageType::CreateBreakpoints:
			GenerateFileBreakPoints(pDebugger, header, messageStream);
			H_DEBUGMESSAGE("Recieved CreateBreakpoints.");
			break;
		case eDebuggerMessageType::CallStack:
			pDebugger->SendCallstack();
			break;
		case eDebuggerMessageType::VariableRequest:
			DecodeVariableRequest(pDebugger, header, messageStream);
			H_DEBUGMESSAGE("Recieved VariableRequest.");
			break;
		case eDebuggerMessageType::EvaluateRequest:
			DecodeEvaluateRequest(pDebugger, header, messageStream);
			H_DEBUGMESSAGE("Recieved EvaluateRequest.");
			break;
		case eDebuggerMessageType::StepIn:
			pDebugger->StepIn();
			H_DEBUGMESSAGE("Recieved StepIn.");
			break;
		case eDebuggerMessageType::StepOver:
			pDebugger->StepOver();
			H_DEBUGMESSAGE("Recieved StepOver.");
			break;
		case eDebuggerMessageType::StepOut:
			pDebugger->StepOut();
			H_DEBUGMESSAGE("Recieved StepOut.");
			break;
		case eDebuggerMessageType::RequestBuildErrors:
			pDebugger->RequestBuildErrors(header);
			H_DEBUGMESSAGE("Recieved Build error request.");
			break;
		case eDebuggerMessageType::RequestEngineTypes:
			pDebugger->RequestEngineTypes(header);
			H_DEBUGMESSAGE("Recieved Engine Types request.");
			break;
		default:
			break;
		}
	}
}

namespace WriteMessages
{
	MessageHeader WriteHeader(int sizeOfMessage, eDebuggerMessageType type, const char* uuid)
	{
		MessageHeader header;
		header.messageLength = sizeOfMessage;
		header.type = type;
		if (uuid)
		{
			memcpy(header.uuid, uuid, 32u);
		}
		else
		{
			memset(header.uuid, 0, 32u);
		}
		return header;
	}

	uint32 EncodeString(MessageData& memoryToFill, const StringL& stringToEncode, uint16 stringLengthOverride = MAX_UINT16)
	{
		uint32 offsetMoved = 0;
		uint16 stringLength = stringLengthOverride == MAX_UINT16 ? stringToEncode.Length() : stringLengthOverride;
		memoryToFill.FillMessageBuffer(&stringLength, sizeof(stringLength));
		offsetMoved += sizeof(stringLength);
		memoryToFill.FillMessageBuffer((void*)stringToEncode.Data(), sizeof(char) * stringLength);
		offsetMoved += sizeof(char) * stringLength;
		return offsetMoved;
	}

	template <typename T>
	void EncodeBaseType(MessageData& memoryToFill, uint32& bufferOffset, T dataToWrite)
	{
		// TODO: Assert if not base type
		memoryToFill.FillMessageBuffer(&dataToWrite, sizeof(T));
		bufferOffset += sizeof(T);
	}
}

void Hail::AngelScript::HandleDebuggerMessage(DebuggerServer* pDebugger, uint32 messageLength, void* messageStream)
{
	uint32 currentPosition = 0;
	while (currentPosition < messageLength)
	{
		MessageHeader header;
		ReadMessages::ReadAndIncrementReadPoint(&header, messageStream, currentPosition, 4 + 32);

		ReadMessages::HandleDebuggerMessageInternal(pDebugger, header, (uint8*)messageStream + currentPosition);
		currentPosition += header.messageLength;
	}
}

DebuggerMessage Hail::AngelScript::CreateStopDebugSessionMessage()
{
	DebuggerMessage message;
	message.m_header = WriteMessages::WriteHeader(0, eDebuggerMessageType::Disconnect, nullptr);
	return message;
}

DebuggerMessage Hail::AngelScript::CreateHitBreakpointMessage(int line, const StringL& file)
{
	DebuggerMessage message;
	message.m_header = WriteMessages::WriteHeader(file.Length() + sizeof(uint16) + sizeof(int), eDebuggerMessageType::HitBreakpoint, nullptr);

	uint32 offset = WriteMessages::EncodeString(message.m_data, file);
	message.m_data.FillMessageBuffer(&line, sizeof(int));
	offset += sizeof(int);
	H_ASSERT(message.m_header.messageLength == offset, "Message length must match final offset");
	return message;
}

DebuggerMessage Hail::AngelScript::CreateStopExecutionMessage()
{
	DebuggerMessage message;
	message.m_header = WriteMessages::WriteHeader(0, eDebuggerMessageType::StopExecution, nullptr);
	return message;
}

DebuggerMessage Hail::AngelScript::CreateCallstackMessage(const GrowingArray<StackFrame>& callstackToSend)
{
	DebuggerMessage message;

	uint32 bufferOffset = 0;
	WriteMessages::EncodeBaseType(message.m_data, bufferOffset, (uint32)callstackToSend.Size());
	for (size_t i = 0; i < callstackToSend.Size(); i++)
	{
		WriteMessages::EncodeBaseType(message.m_data, bufferOffset, callstackToSend[i].m_line);
		bufferOffset += WriteMessages::EncodeString(message.m_data, callstackToSend[i].m_functionName);
		bufferOffset += WriteMessages::EncodeString(message.m_data, callstackToSend[i].m_sourceFile);
	}

	message.m_header = WriteMessages::WriteHeader(bufferOffset, eDebuggerMessageType::CallStack, nullptr);

	H_ASSERT(message.m_header.messageLength == bufferOffset, "Message length must match final offset");
	return message;
}

void EncodeVariable(const Variable& variableToEncode, uint32& currentOffset, MessageData& messageToFill)
{
	currentOffset += WriteMessages::EncodeString(messageToFill, variableToEncode.m_name);
	currentOffset += WriteMessages::EncodeString(messageToFill, variableToEncode.m_type);
	currentOffset += WriteMessages::EncodeString(messageToFill, variableToEncode.m_value);
	uint32 numberOfMembers = variableToEncode.m_members.Size();
	WriteMessages::EncodeBaseType(messageToFill, currentOffset, numberOfMembers);
	for (size_t i = 0; i < numberOfMembers; i++)
	{
		EncodeVariable(variableToEncode.m_members[i], currentOffset, messageToFill);
	}
}

DebuggerMessage Hail::AngelScript::CreateVariablesMessage(const GrowingArray<Variable>* pVariableScopeToSend)
{
	DebuggerMessage message;
	uint32 bufferOffset = 0;
	const uint32 numberOfVariables = pVariableScopeToSend ? pVariableScopeToSend->Size() : 0;
	WriteMessages::EncodeBaseType(message.m_data, bufferOffset, numberOfVariables);
	uint32 numberOfVariablesEncoded = 0;
	for (size_t i = 0; i < numberOfVariables; i++)
	{
		const Variable& variableToWrite = (*pVariableScopeToSend)[i];
		EncodeVariable(variableToWrite, bufferOffset, message.m_data);
	}
	message.m_header = WriteMessages::WriteHeader(bufferOffset, eDebuggerMessageType::VariableRequest, nullptr);
	return message;
}

DebuggerMessage Hail::AngelScript::CreateVariableMessage(const Variable* pVariableScopeToSend)
{
	DebuggerMessage message;
	uint32 bufferOffset = 0;
	if (pVariableScopeToSend)
	{
		EncodeVariable(*pVariableScopeToSend, bufferOffset, message.m_data);
	}
	message.m_header = WriteMessages::WriteHeader(bufferOffset, eDebuggerMessageType::EvaluateRequest, nullptr);
	return message;
}

DebuggerMessage Hail::AngelScript::CreateBuildErrorMessage(const MessageHeader& header, const GrowingArray<BuildErrorInfo>& buildErrorsToSend)
{
	DebuggerMessage message;
	uint32 bufferOffset = 0;

	WriteMessages::EncodeBaseType(message.m_data, bufferOffset, (uint32)buildErrorsToSend.Size());
	for (uint32 i = 0; i < buildErrorsToSend.Size(); i++)
	{
		const BuildErrorInfo& buildError = buildErrorsToSend[i];
		WriteMessages::EncodeBaseType(message.m_data, bufferOffset, buildError.m_col);
		WriteMessages::EncodeBaseType(message.m_data, bufferOffset, buildError.m_row);
		bufferOffset += WriteMessages::EncodeString(message.m_data, buildError.m_section);
		bufferOffset += WriteMessages::EncodeString(message.m_data, buildError.m_message);
	}
	message.m_header = WriteMessages::WriteHeader(bufferOffset, header.type, header.uuid);
	return message;
}

DebuggerMessage Hail::AngelScript::CreateEngineTypeResponseMessage(const MessageHeader& header, TypeDebuggerRegistry* pTypeDebuggerRegistry, const GrowingArray<const char*>& mandatoryIncludePaths)
{
	DebuggerMessage message;
	uint32 bufferOffset = 0;

	WriteMessages::EncodeBaseType(message.m_data, bufferOffset, (uint32)mandatoryIncludePaths.Size());
	for (uint32 iIncludePath = 0; iIncludePath < mandatoryIncludePaths.Size(); iIncludePath++)
	{
		char filePathAsChar[1024];
		memset(filePathAsChar, 0, 1024);
		const FilePath scriptsBaseIncludePath = FilePath::GetAngelscriptDirectory() + StringLW(mandatoryIncludePaths[iIncludePath]).Data();
		FromWCharToConstChar(scriptsBaseIncludePath.Data(), filePathAsChar, 1024);
		bufferOffset += WriteMessages::EncodeString(message.m_data, filePathAsChar);
	}

	const GrowingArray<TypeDebuggerRegistry::Enum>& registeredEnums = pTypeDebuggerRegistry->GetRegisteredEnums();
	WriteMessages::EncodeBaseType(message.m_data, bufferOffset, (uint32)registeredEnums.Size());
	for (uint32 iEnum = 0; iEnum < registeredEnums.Size(); iEnum++)
	{
		const TypeDebuggerRegistry::Enum& registeredEnum = registeredEnums[iEnum];
		bufferOffset += WriteMessages::EncodeString(message.m_data, registeredEnum.m_name.Data());
		WriteMessages::EncodeBaseType(message.m_data, bufferOffset, registeredEnum.m_line);
		bufferOffset += WriteMessages::EncodeString(message.m_data, registeredEnum.m_owningFile.Data());
		WriteMessages::EncodeBaseType(message.m_data, bufferOffset, (uint32)registeredEnum.m_values.Size());
		for (uint32 iEnumValue = 0; iEnumValue < registeredEnum.m_values.Size(); iEnumValue++)
		{
			bufferOffset += WriteMessages::EncodeString(message.m_data, registeredEnum.m_values[iEnumValue].Data());
		}
	}

	const GrowingArray<TypeDebuggerRegistry::GlobalFunction>& registeredGlobalFunctions = pTypeDebuggerRegistry->GetRegisteredFunctions();
	WriteMessages::EncodeBaseType(message.m_data, bufferOffset, (uint32)registeredGlobalFunctions.Size());
	for (uint32 iFunction = 0; iFunction < registeredGlobalFunctions.Size(); iFunction++)
	{
		const TypeDebuggerRegistry::GlobalFunction& registeredFunction = registeredGlobalFunctions[iFunction];
		bufferOffset += WriteMessages::EncodeString(message.m_data, registeredFunction.m_signature.m_type.Data());
		bufferOffset += WriteMessages::EncodeString(message.m_data, registeredFunction.m_signature.m_name.Data());
		WriteMessages::EncodeBaseType(message.m_data, bufferOffset, registeredFunction.m_line);
		bufferOffset += WriteMessages::EncodeString(message.m_data, registeredFunction.m_owningFile.Data());
		WriteMessages::EncodeBaseType(message.m_data, bufferOffset, (uint32)registeredFunction.m_arguments.Size());
		for (uint32 iArgumentVar = 0; iArgumentVar < registeredFunction.m_arguments.Size(); iArgumentVar++)
		{
			bufferOffset += WriteMessages::EncodeString(message.m_data, registeredFunction.m_arguments[iArgumentVar].m_type.Data());
			bufferOffset += WriteMessages::EncodeString(message.m_data, registeredFunction.m_arguments[iArgumentVar].m_name.Data());
		}
	}

	const GrowingArray<TypeDebuggerRegistry::Class>& registeredGlobalClasses = pTypeDebuggerRegistry->GetRegisteredClasses();
	WriteMessages::EncodeBaseType(message.m_data, bufferOffset, (uint32)registeredGlobalClasses.Size());
	for (uint32 iClass = 0; iClass < registeredGlobalClasses.Size(); iClass++)
	{
		const TypeDebuggerRegistry::Class& registeredClass = registeredGlobalClasses[iClass];
		// Remove template type names
		if (int endIndex = StringContainsAndEndsAtIndex(registeredClass.m_name.Data(), "<T>"))
		{
			bufferOffset += WriteMessages::EncodeString(message.m_data, registeredClass.m_name.Data(), endIndex);
		}
		else if(int endIndex = StringContainsAndEndsAtIndex(registeredClass.m_name.Data(), "_t"))
		{
			bufferOffset += WriteMessages::EncodeString(message.m_data, registeredClass.m_name.Data(), endIndex);
		}
		else
		{
			bufferOffset += WriteMessages::EncodeString(message.m_data, registeredClass.m_name.Data());
		}
		WriteMessages::EncodeBaseType(message.m_data, bufferOffset, registeredClass.m_line);
		bufferOffset += WriteMessages::EncodeString(message.m_data, registeredClass.m_owningFile.Data());
		WriteMessages::EncodeBaseType(message.m_data, bufferOffset, (uint32)registeredClass.m_variables.Size());
		for (uint32 iVar = 0; iVar < registeredClass.m_variables.Size(); iVar++)
		{
			const TypeDebuggerRegistry::Class::Variable& var = registeredClass.m_variables[iVar];
			WriteMessages::EncodeBaseType(message.m_data, bufferOffset, var.m_line);
			bufferOffset += WriteMessages::EncodeString(message.m_data, var.m_signature.m_type.Data());
			bufferOffset += WriteMessages::EncodeString(message.m_data, var.m_signature.m_name.Data());
		}
		WriteMessages::EncodeBaseType(message.m_data, bufferOffset, (uint32)registeredClass.m_functions.Size());
		for (uint32 iFunc = 0; iFunc < registeredClass.m_functions.Size(); iFunc++)
		{
			const TypeDebuggerRegistry::Function& func = registeredClass.m_functions[iFunc];
			WriteMessages::EncodeBaseType(message.m_data, bufferOffset, func.m_line);
			bufferOffset += WriteMessages::EncodeString(message.m_data, func.m_signature.m_type.Data());
			bufferOffset += WriteMessages::EncodeString(message.m_data, func.m_signature.m_name.Data());
			WriteMessages::EncodeBaseType(message.m_data, bufferOffset, (uint32)func.m_arguments.Size());
			for (uint32 iArgumentVar = 0; iArgumentVar < func.m_arguments.Size(); iArgumentVar++)
			{
				bufferOffset += WriteMessages::EncodeString(message.m_data, func.m_arguments[iArgumentVar].m_type.Data());
				bufferOffset += WriteMessages::EncodeString(message.m_data, func.m_arguments[iArgumentVar].m_name.Data());
			}
		}
	}

	message.m_header = WriteMessages::WriteHeader(bufferOffset, header.type, header.uuid);
	return message;
}

char* Hail::AngelScript::MessageData::GetMessageData()
{
	return m_extendableMessage.Empty() ? m_message : m_extendableMessage.Data();
}

void Hail::AngelScript::MessageData::FillMessageBuffer(void* dataToFillWith, uint32 numberOfBytesToFill)
{
	if (numberOfBytesToFill + m_dataAmount > 1024)
	{
		if (m_extendableMessage.Empty())
		{
			m_extendableMessage.PrepareAndFill((numberOfBytesToFill + m_dataAmount) * 2);
			memcpy(m_extendableMessage.Data(), m_message, m_dataAmount);
		}
		else if (numberOfBytesToFill + m_dataAmount > m_extendableMessage.Size())
		{
			m_extendableMessage.AddN(numberOfBytesToFill);
		}
	}

	if (m_extendableMessage.Empty())
	{
		memcpy(m_message + m_dataAmount, dataToFillWith, numberOfBytesToFill);
	}
	else
	{
		memcpy(m_extendableMessage.Data() + m_dataAmount, dataToFillWith, numberOfBytesToFill);
	}
	m_dataAmount += numberOfBytesToFill;
}
