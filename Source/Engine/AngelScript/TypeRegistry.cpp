#include "Engine_PCH.h"
#include "TypeRegistry.h"
#include "angelscript.h"

using namespace Hail;

namespace
{
	StringL GetArgumentsFromList(VectorOnStack<AngelScript::VariableTypeData, 8u> argumentVariables)
	{
		StringL arguments;
		for (uint32 i = 0; i < argumentVariables.Size(); i++)
		{
			const AngelScript::VariableTypeData& varTypeData = argumentVariables[i];
			//const Vec2 &in
			if (varTypeData.m_bIsConst)
			{
				arguments += "const ";
			}
			arguments += varTypeData.m_type;
			if (varTypeData.m_bIsRef)
			{
				arguments += "& ";
			}
			else
			{
				arguments += " ";
			}

			if (!varTypeData.m_name.Empty())
			{
				if (varTypeData.m_bReplaceNameWithIn)
				{
					arguments += "in";
				}
				else
				{
					arguments += varTypeData.m_name;
				}
			}

			if (!varTypeData.m_extraArgument.Empty())
			{
				arguments += " ";
				arguments += varTypeData.m_extraArgument;
			}

			if (i + 1 < argumentVariables.Size())
			{
				arguments += ", ";
			}
		}
		return arguments;
	}
	StringL GetClassMethodDefinition(AngelScript::VariableTypeData function, VectorOnStack<AngelScript::VariableTypeData, 8u> argumentVariables)
	{
		StringL declaration;
		StringL arguments = GetArgumentsFromList(argumentVariables);

		if (function.m_bIsConst)
		{
			if (arguments.Length())
			{
				if (function.m_bIsRef)
				{
					declaration = StringL::Format("const %s &%s(%s) const", function.m_type.Data(), function.m_name.Data(), arguments.Data());
				}
				else
				{
					declaration = StringL::Format("%s %s(%s) const", function.m_type.Data(), function.m_name.Data(), arguments.Data());
				}
			}
			else
			{
				if (function.m_bIsRef)
				{
					declaration = StringL::Format("const %s &%s() const", function.m_type.Data(), function.m_name.Data());
				}
				else
				{
					declaration = StringL::Format("%s %s() const", function.m_type.Data(), function.m_name.Data());
				}
			}
		}
		else
		{
			if (arguments.Length())
			{
				if (function.m_bIsRef)
				{
					declaration = StringL::Format("%s &%s(%s)", function.m_type.Data(), function.m_name.Data(), arguments.Data());
				}
				else
				{
					declaration = StringL::Format("%s %s(%s)", function.m_type.Data(), function.m_name.Data(), arguments.Data());
				}
			}
			else
			{
				if (function.m_bIsRef)
				{
					declaration = StringL::Format("%s &%s()", function.m_type.Data(), function.m_name.Data());
				}
				else
				{
					declaration = StringL::Format("%s %s()", function.m_type.Data(), function.m_name.Data());
				}
			}
		}
		return declaration;
	}
	StringL GetClassOperatorDeclaration(const char* typeName, AngelScript::VariableTypeData function)
	{

		StringL declaration;
		if (function.m_bIsConst)
		{
			// bool opEquals(const Vec2 &in) const

			if (function.m_bIsRef)
			{
				declaration = StringL::Format("%s &%s(const %s &in) const", function.m_type, function.m_name, typeName);
			}
			else
			{
				declaration = StringL::Format("%s %s(const %s &in) const", function.m_type, function.m_name, typeName);
			}
		}
		else
		{
			//Vec2 &opAddAssign(const Vec2 &in)
			if (function.m_bIsRef)
			{
				declaration = StringL::Format("%s &%s(const %s &in)", function.m_type, function.m_name, typeName);
			}
			else
			{
				declaration = StringL::Format("%s %s(const %s &in)", function.m_type, function.m_name, typeName);
			}
		}
		return declaration;
	}
}

Hail::AngelScript::TypeRegistry::TypeRegistry(asIScriptEngine* pAsEngine, bool bEnableDebugger)
	: m_pScriptEngine(pAsEngine)
{
	if (bEnableDebugger) 
	{
		m_pDebuggerRegistry = new TypeDebuggerRegistry();
	}
}

bool Hail::AngelScript::TypeRegistry::RegisterType(const char* typeName, uint32 sizeOfType, uint64 flags, const char* sourceFileName, int line, const char* registerNameOverride)
{
	for (uint32 i = 0; i < m_registeredTypes.Size(); i++)
	{
		if (m_registeredTypes[i].nameID == typeName)
		{
			H_ERROR(StringL::Format("Already awssigned angelscript object %s: ", typeName));
			return true;
		}
	}

	AsTypeIDNamePair objectToRegister;
	objectToRegister.nameID = typeName;
	objectToRegister.bRegisteredVariableFetchFunction = false;

	int r = m_pScriptEngine->RegisterObjectType(typeName, sizeOfType, flags); assert(r >= 0);
	objectToRegister.typeID = m_pScriptEngine->GetTypeIdByDecl(typeName);
	m_registeredTypes.Add(objectToRegister);

	if (r >= 0 && m_pDebuggerRegistry)
	{
		m_pDebuggerRegistry->RegisterClass(registerNameOverride ? registerNameOverride : typeName, sourceFileName, line);
	}

    return r >= 0;
}

bool Hail::AngelScript::TypeRegistry::RegisterVariableFunction(const char* typeName, ToVariableCallback callbackToRegister)
{
	for (uint32 i = 0; i < m_registeredTypes.Size(); i++)
	{
		if (StringCompare(m_registeredTypes[i].nameID, typeName))
		{
			m_registeredTypes[i].toVariableCallback = callbackToRegister;
			m_registeredTypes[i].bRegisteredVariableFetchFunction = true;
			return true;
		}
	}

	H_ERROR("Trying to assign a ToVariable callback to an unregistered object type.");

    return false;
}

bool Hail::AngelScript::TypeRegistry::RegisterClassMethod(const char* typeName, VariableTypeData function, VectorOnStack<VariableTypeData, 8u> argumentVariables, const asSFuncPtr& funcPointer, const char* sourceFileName, int line)
{
	StringL declaration = GetClassMethodDefinition(function, argumentVariables);

	int r = m_pScriptEngine->RegisterObjectMethod(typeName, declaration.Data(), funcPointer, asCALL_THISCALL);

	if (r >= 0 && m_pDebuggerRegistry)
	{
		m_pDebuggerRegistry->RegisterClassMethod(typeName, function, argumentVariables, sourceFileName, line);
	}

	return r >= 0;
}

bool Hail::AngelScript::TypeRegistry::RegisterClassMethodGenericFuncPtr(const char* typeName, VariableTypeData function, VectorOnStack<VariableTypeData, 8u> argumentVariables, const asSFuncPtr& funcPointer, const char* sourceFileName, int line)
{
	StringL declaration = GetClassMethodDefinition(function, argumentVariables);

	int r = m_pScriptEngine->RegisterObjectMethod(typeName, declaration.Data(), funcPointer, asCALL_GENERIC);

	if (r >= 0 && m_pDebuggerRegistry)
	{
		m_pDebuggerRegistry->RegisterClassMethod(typeName, function, argumentVariables, sourceFileName, line);
	}

	return r >= 0;
}

bool Hail::AngelScript::TypeRegistry::RegisterClassGetSetter(const char* typeName, VariableTypeData function, const asSFuncPtr& funcPointer, bool bIsSetter, const char* sourceFileName, int line)
{
	StringL declaration;
	if (bIsSetter)
	{
		//Example "Vec2", "void set_x(float) property
		declaration = StringL::Format("void set_%s(%s) property", function.m_name, function.m_type);
	}
	else
	{
		//Example "Vec2", "float get_y() const property"
		declaration = StringL::Format("%s get_%s() const property", function.m_type, function.m_name);
	}

	int r = m_pScriptEngine->RegisterObjectMethod(typeName, declaration.Data(), funcPointer, asCALL_THISCALL);

	if (r >= 0 && m_pDebuggerRegistry)
	{
		// TODO: If a setting is set, cache all the above data to send to debugger

	}

	return r >= 0;
}

bool Hail::AngelScript::TypeRegistry::RegisterClassOperatorOverload(const char* typeName, VariableTypeData function, const asSFuncPtr& funcPointer, const char* sourceFileName, int line)
{
	StringL declaration = GetClassOperatorDeclaration(typeName, function);
	int r = m_pScriptEngine->RegisterObjectMethod(typeName, declaration.Data(), funcPointer, asCALL_THISCALL);

	if (r >= 0 && m_pDebuggerRegistry)
	{
		m_pDebuggerRegistry->RegisterClassMethod(typeName, function, {}, sourceFileName, line);
	}

	return r >= 0;
}

bool Hail::AngelScript::TypeRegistry::RegisterClassOperatorOverloadGenericFuncPtr(const char* typeName, VariableTypeData function, const asSFuncPtr& funcPointer, const char* sourceFileName, int line)
{
	StringL declaration = GetClassOperatorDeclaration(typeName, function);
	int r = m_pScriptEngine->RegisterObjectMethod(typeName, declaration.Data(), funcPointer, asCALL_GENERIC);

	if (r >= 0 && m_pDebuggerRegistry)
	{
		m_pDebuggerRegistry->RegisterClassMethod(typeName, function, {}, sourceFileName, line);
	}

	return r >= 0;
}

bool Hail::AngelScript::TypeRegistry::RegisterClassObjectMember(const char* typeName, VariableTypeData memberVariable, int byteOffset, const char* sourceFileName, int line)
{
	StringL object = StringL::Format("%s %s", memberVariable.m_type, memberVariable.m_name);

	int r = m_pScriptEngine->RegisterObjectProperty(typeName, object.Data(), byteOffset);
	if (r >= 0 && m_pDebuggerRegistry)
	{
		m_pDebuggerRegistry->RegisterClassObjectMember(typeName, memberVariable, sourceFileName, line);

	}
	return r >= 0;
}

bool Hail::AngelScript::TypeRegistry::RegisterClassConstructor(const char* typeName, VectorOnStack<VariableTypeData, 8u> argumentVariables, VectorOnStack<VariableTypeData, 8u> listArguments, const asSFuncPtr& funcPointer, bool bConstroctur, const char* sourceFileName, int line)
{
	StringL constructor;

	if (argumentVariables.Empty() && listArguments.Empty())
	{
		constructor = "void f()";
	}
	else if (!argumentVariables.Empty() && listArguments.Empty())
	{
		const StringL constructorArguments = GetArgumentsFromList(argumentVariables);
		constructor = StringL::Format("void f(%s)", constructorArguments.Data());
	}
	else if(!argumentVariables.Empty() && !listArguments.Empty())
	{
		const StringL constructorArguments = GetArgumentsFromList(argumentVariables);
		const StringL initListArguments = GetArgumentsFromList(listArguments);
		constructor = StringL::Format("void f(%s) {%s}", constructorArguments.Data(), initListArguments.Data());
	}
	else
	{
		H_ASSERT(false);
	}
	/*
	r = m_pScriptEngine->RegisterObjectBehaviour("Vec2", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Vec2DefaultConstructor), asCALL_CDECL_OBJLAST); H_ASSERT(r >= 0, "Failed to register Vec2 func");
	r = m_pScriptEngine->RegisterObjectBehaviour("Vec2", asBEHAVE_CONSTRUCT, "void f(const Vec2 &in)", asFUNCTION(Vec2CopyConstructor), asCALL_CDECL_OBJLAST); H_ASSERT(r >= 0, "Failed to register Vec2 func");
	r = m_pScriptEngine->RegisterObjectBehaviour("Vec2", asBEHAVE_CONSTRUCT, "void f(float)", asFUNCTION(Vec2ConvConstructor), asCALL_CDECL_OBJLAST); H_ASSERT(r >= 0, "Failed to register Vec2 func");
	r = m_pScriptEngine->RegisterObjectBehaviour("Vec2", asBEHAVE_CONSTRUCT, "void f(float, float)", asFUNCTION(Vec2InitConstructor), asCALL_CDECL_OBJLAST); H_ASSERT(r >= 0, "Failed to register Vec2 func");
	r = m_pScriptEngine->RegisterObjectBehaviour("Vec2", asBEHAVE_LIST_CONSTRUCT, "void f(const int &in) {float, float}", asFUNCTION(Vec2ListConstructor), asCALL_CDECL_OBJLAST); H_ASSERT(r >= 0, "Failed to register Vec2 func");
	*/
	const asEBehaviours behavior = listArguments.Empty() ? asBEHAVE_CONSTRUCT : asBEHAVE_LIST_CONSTRUCT;
	int r = m_pScriptEngine->RegisterObjectBehaviour(typeName, bConstroctur ? behavior : asBEHAVE_DESTRUCT, constructor.Data(), funcPointer, asCALL_CDECL_OBJLAST);
	if (r >= 0 && bConstroctur  && m_pDebuggerRegistry)
	{
		VariableTypeData constructorDefinition;
		constructorDefinition.m_name = typeName;
		constructorDefinition.m_type = "constructor";
		m_pDebuggerRegistry->RegisterClassMethod(typeName, constructorDefinition, argumentVariables, sourceFileName, line);
	}
	return r >= 0;
}

bool Hail::AngelScript::TypeRegistry::RegisterManagedClassConstructor(const char* typeName, const StringL& constructor, VectorOnStack<VariableTypeData, 8u> argumentVariables, int angelscriptFlags, int angelscriptCallConvention, const asSFuncPtr& funcPointer, const char* sourceFileName, int line)
{
	StringL finalConstructor;

	if (argumentVariables.Empty())
	{
		finalConstructor = constructor;
	}
	else
	{
		const StringL constructorArguments = GetArgumentsFromList(argumentVariables);
		finalConstructor = StringL::Format(constructor, constructorArguments.Data());
	}

	int r = m_pScriptEngine->RegisterObjectBehaviour(typeName, (asEBehaviours)angelscriptFlags, finalConstructor.Data(), funcPointer, angelscriptCallConvention);
	if (r >= 0 && m_pDebuggerRegistry)
	{
		VariableTypeData constructorDefinition;
		constructorDefinition.m_name = typeName;
		constructorDefinition.m_type = "constructor";
		m_pDebuggerRegistry->RegisterClassMethod(typeName, constructorDefinition, argumentVariables, sourceFileName, line);
	}
	return r >= 0;
}

bool Hail::AngelScript::TypeRegistry::RegisterGlobalMethod(VariableTypeData function, VectorOnStack<VariableTypeData, 8u> argumentVariables, const asSFuncPtr& funcPointer, const char* sourceFileName, int line)
{
	//m_pScriptEngine->RegisterGlobalFunction("Vec2 GetDirectionInput(eInputAction, int)", asFUNCTION(localGetDirectionInput), asCALL_CDECL)
	const StringL arguments = GetArgumentsFromList(argumentVariables);
	const StringL globalFunction = StringL::Format("%s %s(%s)", function.m_type.Data(), function.m_name.Data(), arguments.Data());
	int r = m_pScriptEngine->RegisterGlobalFunction(globalFunction.Data(), funcPointer, asCALL_CDECL);
	if (r >= 0 && m_pDebuggerRegistry)
	{
		m_pDebuggerRegistry->RegisterGlobalMethod(function, argumentVariables, sourceFileName, line);
	}
	return r >= 0;
}

bool Hail::AngelScript::TypeRegistry::RegisterGlobalEnumValue(const char* name, const char* valueName, uint32 value, const char* sourceFileName, int line)
{
	int r = m_pScriptEngine->RegisterEnumValue(name, valueName, value);
	if (r >= 0 && m_pDebuggerRegistry)
	{
		m_pDebuggerRegistry->RegisterGlobalEnumValue(name, valueName, value, sourceFileName, line);
	}
	return r >= 0;
}

Hail::AngelScript::Variable Hail::AngelScript::TypeRegistry::GetVariableFromCallback(uint32 typeID, void* pObject)
{
	for (uint32 i = 0; i < m_registeredTypes.Size(); i++)
	{
		if (m_registeredTypes[i].typeID == typeID)
		{
			H_ASSERT(m_registeredTypes[i].bRegisteredVariableFetchFunction, StringL::Format("Unregistered callback invoked, add a callbasck to type %s", m_registeredTypes[i].nameID.Data()));
			return m_registeredTypes[i].toVariableCallback(pObject);
		}
	}
	const char* declaration = m_pScriptEngine->GetTypeDeclaration(typeID);
	const uint32 declarationLength = StringLength(declaration);
	if (declaration[declarationLength - 2] == '[' && declaration[declarationLength - 1] == ']')
	{
		// 0 is for arrays
		return m_registeredTypes[0].toVariableCallback(pObject);
	}

	return Variable();
}

void Hail::AngelScript::TypeDebuggerRegistry::RegisterClass(const char* className, const char* sourceFileName, int line)
{
	for (uint32 i = 0; i < m_registeredClasses.Size(); i++)
	{
		if (StringCompare(className, m_registeredClasses[i].m_name))
		{
			H_ASSERT(false, "Can not register the same class twice");
			return;
		}
	}
	Class& registeredClass = m_registeredClasses.Add();
	registeredClass.m_name = className;
	registeredClass.m_owningFile = sourceFileName;
	registeredClass.m_line = line;
}

void Hail::AngelScript::TypeDebuggerRegistry::RegisterClassMethod(const char* className, VariableTypeData function, VectorOnStack<VariableTypeData, 8u> argumentVariables, const char* sourceFileName, int line)
{
	Class* pRegisteredClass = nullptr;
	for (uint32 i = 0; i < m_registeredClasses.Size(); i++)
	{
		if (StringCompare(className, m_registeredClasses[i].m_name))
		{
			H_ASSERT(StringCompare(sourceFileName, m_registeredClasses[i].m_owningFile.Data()), "Must register the class in the same file as its functions");
			pRegisteredClass = &m_registeredClasses[i];
		}
	}
	if (!pRegisteredClass)
	{
		H_ASSERT(pRegisteredClass, StringL::Format("No class registered for %s", className))
		return;
	}

	Function& registeredFunction = pRegisteredClass->m_functions.Add();
	registeredFunction.m_line = line;
	registeredFunction.m_arguments = argumentVariables;
	registeredFunction.m_signature = function;
}

void Hail::AngelScript::TypeDebuggerRegistry::RegisterClassObjectMember(const char* className, VariableTypeData memberVariable, const char* sourceFileName, int line)
{
	Class* pRegisteredClass = nullptr;
	for (uint32 i = 0; i < m_registeredClasses.Size(); i++)
	{
		if (StringCompare(className, m_registeredClasses[i].m_name))
		{
			pRegisteredClass = &m_registeredClasses[i];
		}
	}
	if (!pRegisteredClass)
	{
		H_ASSERT(false, "Class is not registered");
		return;
	}
	H_ASSERT(StringCompare(sourceFileName, pRegisteredClass->m_owningFile.Data()), "Must register in members in the same file");
	Class::Variable& registeredVariable = pRegisteredClass->m_variables.Add();
	registeredVariable.m_signature = memberVariable;
	registeredVariable.m_line = line;
}

void Hail::AngelScript::TypeDebuggerRegistry::RegisterGlobalMethod(VariableTypeData function, VectorOnStack<VariableTypeData, 8u> argumentVariables, const char* sourceFileName, int line)
{
	GlobalFunction& registeredGlobalFunction = m_regiseredGlobalFunctions.Add();
	registeredGlobalFunction.m_arguments = argumentVariables;
	registeredGlobalFunction.m_signature = function;
	registeredGlobalFunction.m_owningFile = sourceFileName;
	registeredGlobalFunction.m_line = line;
}

void Hail::AngelScript::TypeDebuggerRegistry::RegisterGlobalEnumValue(const char* name, const char* valueName, uint32 value, const char* sourceFileName, int line)
{
	int32 registeredEnumIndex = -1;
	for (uint32 i = 0; i < m_regiseredGlobalEnums.Size(); i++)
	{
		if (StringCompare(m_regiseredGlobalEnums[i].m_name, name)) 
		{
			registeredEnumIndex = i;
			continue;
		}
	}

	if ((registeredEnumIndex == -1 && value != 0))
	{
		H_ASSERT(false, "Enum must be registered with a 0 value first");
		return;
	}

	GrowingArray<String64>* pRegisteredGlobalEnumValues = nullptr;
	if (registeredEnumIndex == -1)
	{
		Enum& registeredGlobalEnum = m_regiseredGlobalEnums.Add();
		registeredGlobalEnum.m_name = name;
		registeredGlobalEnum.m_owningFile = sourceFileName;
		registeredGlobalEnum.m_line = line;
		pRegisteredGlobalEnumValues = &registeredGlobalEnum.m_values;
	}
	else
	{
		Enum& registeredGlobalEnum = m_regiseredGlobalEnums[registeredEnumIndex];
		pRegisteredGlobalEnumValues = &registeredGlobalEnum.m_values;
	}
	pRegisteredGlobalEnumValues->Add(valueName);
}
