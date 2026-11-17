#include "Game_PCH.h"
#include "DebuggerTypes.h"


namespace Hail
{
	namespace AngelScript
	{
		Variable GetVariableBool(const char* pName, bool value)
		{
			AngelScript::Variable variable;
			variable.m_name = pName;
			variable.m_type = "bool";
			const char* charTrue = "true";
			const char* charFalse = "false";
			variable.m_value = StringL::Format("%s %s", pName, value ? charTrue : charFalse);
			return variable;
		}

		Variable AngelScript::GetVariableInt32(const char* pName, int32 value)
		{
			AngelScript::Variable variable;
			variable.m_name = pName;
			variable.m_type = "int";
			variable.m_value = StringL::Format("%s %i", pName, value);
			return variable;
		}

		Variable GetVariableUint32(const char* pName, uint32 value)
		{
			AngelScript::Variable variable;
			variable.m_name = pName;
			variable.m_type = "uint32";
			variable.m_value = StringL::Format("%s %u", pName, value);
			return variable;
		}

		Variable GetVariableFloat(const char* pName, float value)
		{
			AngelScript::Variable variable;
			variable.m_name = pName;
			variable.m_type = "float";
			variable.m_value = StringL::Format("%s %f", pName, value);
			return variable;
		}

		Variable GetVariableVec2(const char* pName, glm::vec2 value)
		{
			AngelScript::Variable variable;
			variable.m_name = pName;
			variable.m_type = "Vec2";
			variable.m_value = StringL::Format("x %f, y %f", value.x, value.y);
			variable.m_members.Add(GetVariableFloat("x", value.x));
			variable.m_members.Add(GetVariableFloat("y", value.y));
			return variable;
		}

		Variable GetVariableVec3(const char* pName, glm::vec3 value)
		{
			AngelScript::Variable variable;
			variable.m_name = pName;
			variable.m_type = "Vec3";
			variable.m_value = StringL::Format("x %f, y %f, z %f", value.x, value.y, value.z);
			variable.m_members.Add(GetVariableFloat("x", value.x));
			variable.m_members.Add(GetVariableFloat("y", value.y));
			variable.m_members.Add(GetVariableFloat("z", value.z));
			return variable;
		}

		Variable GetVariableVec4(const char* pName, glm::vec4 value)
		{
			AngelScript::Variable variable;
			variable.m_name = pName;
			variable.m_type = "Vec4";
			variable.m_value = StringL::Format("x %f, y %f, z %f, w %f", value.x, value.y, value.z, value.w);
			variable.m_members.Add(GetVariableFloat("x", value.x));
			variable.m_members.Add(GetVariableFloat("y", value.y));
			variable.m_members.Add(GetVariableFloat("z", value.z));
			variable.m_members.Add(GetVariableFloat("w", value.w));
			return variable;
		}
	}

}