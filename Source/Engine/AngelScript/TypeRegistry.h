#pragma once
#include "Types.h"
#include "Containers\GrowingArray\GrowingArray.h"
#include "String.hpp"
#include "DebuggerTypes.h"

class asIScriptEngine;
struct asSFuncPtr;

namespace Hail
{
	namespace AngelScript
	{
		enum class EVariableTypeDataState
		{
			Const,
			Ref,
			ConstRef,
			None
		};
		// function argument (Type Name, )
		struct VariableTypeData
		{
			VariableTypeData() = default;

			VariableTypeData(const char* name, const char* type, bool isConst = false, bool isRef = false, bool replaceNameWithIn = false) :
				m_name(name),
				m_type(type),
				m_bIsConst(isConst),
				m_bIsRef(isRef),
				m_bReplaceNameWithIn(replaceNameWithIn)
			{
			}
			VariableTypeData(const char* name, const char* type, EVariableTypeDataState state, const char* extraArgument = "") :
				m_name(name),
				m_type(type),
				m_extraArgument(extraArgument)
			{
				if (state == EVariableTypeDataState::Const)
				{
					m_bIsConst = true;
				}
				else if (state == EVariableTypeDataState::Ref)
				{
					m_bIsRef = true;
				}
				else if (state == EVariableTypeDataState::ConstRef)
				{
					m_bIsConst = true;
					m_bIsRef = true;
				}
			}
			String64 m_name;
			String64 m_type;
			bool m_bIsConst = false;
			// Only used for arguments
			bool m_bIsRef = false;
			bool m_bReplaceNameWithIn = false;
			String64 m_extraArgument;
		};

		class TypeDebuggerRegistry
		{
		public: 
			struct Function
			{
				int m_line;
				VariableTypeData m_signature;
				VectorOnStack<VariableTypeData, 8u> m_arguments;
			};

			struct GlobalFunction
			{
				int m_line;
				VariableTypeData m_signature;
				VectorOnStack<VariableTypeData, 8u> m_arguments;
				StringL m_owningFile;
			};


			struct Class
			{
				struct Variable
				{
					int m_line;
					VariableTypeData m_signature;
				};
				StringL m_owningFile;
				int m_line;
				String64 m_name;
				GrowingArray<Variable> m_variables;
				GrowingArray<Function> m_functions;
			};

			struct Enum
			{
				String64 m_name;
				uint32 m_line;
				StringL m_owningFile;
				GrowingArray<String64> m_values;
			};

			void RegisterClass(const char* className, const char* sourceFileName, int line);
			void RegisterClassMethod(const char* className, VariableTypeData function, VectorOnStack<VariableTypeData, 8u> argumentVariables, const char* sourceFileName, int line);
			void RegisterClassObjectMember(const char* className, VariableTypeData memberVariable, const char* sourceFileName, int line);

			void RegisterGlobalMethod(VariableTypeData function, VectorOnStack<VariableTypeData, 8u> argumentVariables, const char* sourceFileName, int line);
			void RegisterGlobalEnumValue(const char* name, const char* valueName, uint32 value, const char* sourceFileName, int line);

			const GrowingArray<Class>& GetRegisteredClasses() const { return m_registeredClasses; }
			const GrowingArray<GlobalFunction>& GetRegisteredFunctions() const { return m_regiseredGlobalFunctions; }
			const GrowingArray<Enum>& GetRegisteredEnums() const { return m_regiseredGlobalEnums; }

		private:

			GrowingArray<Class> m_registeredClasses;
			GrowingArray<GlobalFunction> m_regiseredGlobalFunctions;
			GrowingArray<Enum> m_regiseredGlobalEnums;
		};

		class TypeRegistry
		{
		public:
			explicit TypeRegistry(asIScriptEngine* pAsEngine, bool bEnableDebugger);

			// Note: const char* registerNameOverride is for template types as the register name is not the same as the Object name, f I add more template types I will revisit this. 
			bool RegisterType(const char* typeName, uint32 sizeOfType, uint64 flags, const char* sourceFileName, int line, const char* registerNameOverride = nullptr);
			bool RegisterVariableFunction(const char* typeName, ToVariableCallback callbackToRegister);

			bool RegisterClassMethod(const char* typeName, VariableTypeData function, VectorOnStack<VariableTypeData, 8u> argumentVariables, const asSFuncPtr& funcPointer, const char* sourceFileName, int line);
			bool RegisterClassMethodGenericFuncPtr(const char* typeName, VariableTypeData function, VectorOnStack<VariableTypeData, 8u> argumentVariables, const asSFuncPtr& funcPointer, const char* sourceFileName, int line);
			// Creates setters and getters for properties: "void set_x(float) property". Like exampleVector.x or exampleVector.y = 10.0
			// Does not get sent to Angelscript LSP
			bool RegisterClassGetSetter(const char* typeName, VariableTypeData function, const asSFuncPtr& funcPointer, bool bIsSetter, const char* sourceFileName, int line);
			bool RegisterClassOperatorOverload(const char* typeName, VariableTypeData function, const asSFuncPtr& funcPointer, const char* sourceFileName, int line);
			bool RegisterClassOperatorOverloadGenericFuncPtr(const char* typeName, VariableTypeData function, const asSFuncPtr& funcPointer, const char* sourceFileName, int line);
			bool RegisterClassObjectMember(const char* typeName, VariableTypeData memberVariable, int byteOffset, const char* sourceFileName, int line);
			// if bConstroctur == false it is a destructor
			bool RegisterClassConstructor(const char* typeName, VectorOnStack<VariableTypeData, 8u> argumentVariables, VectorOnStack<VariableTypeData, 8u> listArguments, const asSFuncPtr& funcPointer, bool bConstroctur, const char* sourceFileName, int line);
			bool RegisterManagedClassConstructor(const char* typeName, const StringL& constructor, VectorOnStack<VariableTypeData, 8u> argumentVariables, int angelscriptFlags, int angelscriptCallConvention, const asSFuncPtr& funcPointer, const char* sourceFileName, int line);

			bool RegisterGlobalMethod(VariableTypeData function, VectorOnStack<VariableTypeData, 8u> argumentVariables, const asSFuncPtr& funcPointer, const char* sourceFileName, int line);
			bool RegisterGlobalEnumValue(const char* name, const char* valueName, uint32 value, const char* sourceFileName, int line);

			Variable GetVariableFromCallback(uint32 typeID, void* pObject);
			asIScriptEngine* GetEngine() { return m_pScriptEngine; }
			TypeDebuggerRegistry* GetDebuggerRegistry() { return m_pDebuggerRegistry; }
		private:

			struct AsTypeIDNamePair
			{
				uint32 typeID;
				StringL nameID;
				bool bRegisteredVariableFetchFunction;
				ToVariableCallback toVariableCallback;
			};

			GrowingArray<AsTypeIDNamePair> m_registeredTypes;
			//TODO: When we have a hashmap, register hashmap with TypeID to variable function ptrs
			GrowingArray<ToVariableCallback> m_registeredToCallBackFunctions;

			asIScriptEngine* m_pScriptEngine;
			// If a debugger is enabled this will store all the data for the LSP debugger
			TypeDebuggerRegistry* m_pDebuggerRegistry;
		};

#define H_FILE_LINE __FILE__, __LINE__
	}
}