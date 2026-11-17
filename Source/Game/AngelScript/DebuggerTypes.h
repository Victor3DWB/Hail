#pragma once
#include "Types.h"

#include "Containers\GrowingArray\GrowingArray.h"

class asIScriptContext;

namespace Hail
{
	namespace AngelScript
	{
		struct BreakPoint
		{
			int16 line;
			bool bHasConditional;
			StringL condition;
		};

		struct FileBreakPoints
		{
			StringL fileName;
			GrowingArray<BreakPoint> breakPoints;
		};

		struct StackFrame
		{
			uint32 m_line;
			StringL m_functionName;
			StringL m_sourceFile;
		};

		enum class eCallStack : uint8
		{
			global,
			self, // this
			local,
			count
		};

		struct Variable
		{
			StringL m_name;
			StringL m_value;
			StringL m_type;
			GrowingArray<Variable> m_members;
		};

		Variable GetVariableBool(const char* pName, bool value);
		Variable GetVariableInt32(const char* pName, int32 value);
		Variable GetVariableUint32(const char* pName, uint32 value);
		Variable GetVariableFloat(const char* pName, float value);
		Variable GetVariableVec2(const char* pName, glm::vec2 value);
		Variable GetVariableVec3(const char* pName, glm::vec3 value);
		Variable GetVariableVec4(const char* pName, glm::vec4 value);

		typedef Variable(*ToVariableCallback)(void* obj);

		struct BuildErrorInfo
		{
			uint32 m_col;
			uint32 m_row;
			StringL m_section;
			StringL m_message;
		};

	}

}