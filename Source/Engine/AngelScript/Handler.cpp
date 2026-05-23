#include "Engine_PCH.h"
#include "Handler.h"
#include "angelscript.h"
#include "Scriptstdstring.h"
#include "Array.h"
#include "Math.h"
#include "TypeRegistry.h"

#include <new>
#include "Input\InputActionList.h"
#include "Input\InputActionMap.h"
#include "Utility\DebugLineHelpers.h"
#include "ThreadSynchronizer.h"
#include <string>

using namespace Hail;
using namespace AngelScript;

namespace
{
	InputActionMap* g_pInputActionMap;
	ThreadSyncronizer* g_pThreadSynchronizer;

	const char* localGetInputActionMapValueName(eInputAction inputAction)
	{
		switch (inputAction)
		{
		case Hail::eInputAction::PlayerMoveUp:
			return "PlayerMoveUp";
		case Hail::eInputAction::PlayerMoveRight:
			return "PlayerMoveRight";
		case Hail::eInputAction::PlayerMoveLeft:
			return "PlayerMoveLeft";
		case Hail::eInputAction::PlayerMoveDown:
			return "PlayerMoveDown";
		case Hail::eInputAction::PlayerMoveJoystickL:
			return "PlayerMoveJoystickL";
		case Hail::eInputAction::PlayerMoveJoystickR:
			return "PlayerMoveJoystickR";
		case Hail::eInputAction::PlayerAction1:
			return "PlayerAction1";
		case Hail::eInputAction::PlayerAction2:
			return "PlayerAction2";
		case Hail::eInputAction::PlayerPause:
			return "PlayerPause";
		case Hail::eInputAction::DebugAction1:
			return "DebugAction1";
		case Hail::eInputAction::DebugAction2:
			return "DebugAction2";
		case Hail::eInputAction::DebugAction3:
			return "DebugAction3";
		case Hail::eInputAction::DebugAction4:
			return "DebugAction4";
		case Hail::eInputAction::InputCount:
		default:
			break;
		}
		return "";
	}

	static Vec2 localGetDirectionInput(eInputAction actionToGet, int gamepadToCheck)
	{
		const glm::vec2 direction = g_pInputActionMap->GetDirectionInput(actionToGet, gamepadToCheck);
		return Vec2(direction);
	}
	static int localGetButtonInput(eInputAction actionToGet, int gamepadToCheck)
	{
		return (int)g_pInputActionMap->GetButtonInput(actionToGet, gamepadToCheck);
	}
	static float localGetGamepadTriggerInput(eInputAction actionToGet, int gamepadToCheck)
	{
		return g_pInputActionMap->GetGamepadTriggerInput(actionToGet, gamepadToCheck);
	}

	//TODO: replace with cusotm string class
	static void localPrint(std::string& stringToPrint)
	{
		H_DEBUGMESSAGE(stringToPrint.c_str());
	}
	static void localPrintError(std::string& stringToPrint)
	{
		H_ERROR(stringToPrint.c_str());
	}
	static void localPrintWarning(std::string& stringToPrint)
	{
		H_WARNING(stringToPrint.c_str());
	}

	// All non normalized functions work in pixel coordinates.
	//TODO: Add a color type to AngelScript
	static void localDrawLineNormalized(const Vec2& startPos, const Vec2& endPos)
	{
		DrawLine2D(*g_pThreadSynchronizer->GetAppFrameData().commandPoolToFill, startPos.m_vec, endPos.m_vec, true);
	}

	static void localDrawLineNormalizedScreenAlligned(const Vec2& startPos, const Vec2& endPos)
	{
		DrawLine2D(*g_pThreadSynchronizer->GetAppFrameData().commandPoolToFill, startPos.m_vec, endPos.m_vec, false);
	}

	static void localDrawLine(const Vec2& startPos, const Vec2& endPos)
	{
		DrawLine2DPixelSpace(*g_pThreadSynchronizer->GetAppFrameData().commandPoolToFill, startPos.m_vec, endPos.m_vec, true);
	}

	static void localDrawLineScreenAlligned(const Vec2& startPos, const Vec2& endPos)
	{
		DrawLine2DPixelSpace(*g_pThreadSynchronizer->GetAppFrameData().commandPoolToFill, startPos.m_vec, endPos.m_vec, false);
	}

	static void localDrawLineNormalizedRotLength(const Vec2& startPos, float rotationRadian, float length)
	{
		DrawLine2D(*g_pThreadSynchronizer->GetAppFrameData().commandPoolToFill, startPos.m_vec, rotationRadian, length, true);
	}

	static void localDrawLineNormalizedRotLengthScreenAlligned(const Vec2& startPos, float rotationRadian, float length)
	{
		DrawLine2D(*g_pThreadSynchronizer->GetAppFrameData().commandPoolToFill, startPos.m_vec, rotationRadian, length, false);
	}

	static void localDrawLineRotLength(const Vec2& startPos, float rotationRadian, float length)
	{
		DrawLine2DPixelSpace(*g_pThreadSynchronizer->GetAppFrameData().commandPoolToFill, startPos.m_vec, rotationRadian, length, true);
	}

	static void localDrawLineRotLengthScreenAlligned(const Vec2& startPos, float rotationRadian, float length)
	{
		DrawLine2DPixelSpace(*g_pThreadSynchronizer->GetAppFrameData().commandPoolToFill, startPos.m_vec, rotationRadian, length, false);
	}

	static void localDrawBoxNormalized(const Vec2& pos, const Vec2& dimensions)
	{
		DrawBox2D(*g_pThreadSynchronizer->GetAppFrameData().commandPoolToFill, pos.m_vec, dimensions.m_vec, true);
	}

	static void localDrawBoxNormalizedScreenAlligned(const Vec2& pos, const Vec2& dimensions)
	{
		DrawBox2D(*g_pThreadSynchronizer->GetAppFrameData().commandPoolToFill, pos.m_vec, dimensions.m_vec, false);
	}

	static void localDrawBox(const Vec2& pos, const Vec2& dimensions)
	{
		DrawBox2DPixelSpace(*g_pThreadSynchronizer->GetAppFrameData().commandPoolToFill, pos.m_vec, dimensions.m_vec, true);
	}

	static void localDrawBoxScreenAlligned(const Vec2& pos, const Vec2& dimensions)
	{
		DrawBox2DPixelSpace(*g_pThreadSynchronizer->GetAppFrameData().commandPoolToFill, pos.m_vec, dimensions.m_vec, false);
	}

	static void localDrawBoxMinMaxNormalized(const Vec2& min, const Vec2& max)
	{
		DrawRect2D(*g_pThreadSynchronizer->GetAppFrameData().commandPoolToFill, min.m_vec, max.m_vec, true);
	}

	static void localDrawBoxMinMaxNormalizedScreenAlligned(const Vec2& min, const Vec2& max)
	{
		DrawRect2D(*g_pThreadSynchronizer->GetAppFrameData().commandPoolToFill, min.m_vec, max.m_vec, false);
	}

	static void localDrawBoxMinMax(const Vec2& min, const Vec2& max)
	{
		DrawRect2DPixelSpace(*g_pThreadSynchronizer->GetAppFrameData().commandPoolToFill, min.m_vec, max.m_vec, true);
	}

	static void localDrawBoxMinMaxScreenAlligned(const Vec2& min, const Vec2& max)
	{
		DrawRect2DPixelSpace(*g_pThreadSynchronizer->GetAppFrameData().commandPoolToFill, min.m_vec, max.m_vec, false);
	}

	static void localDrawCircleNormalized(const Vec2& pos, float radius)
	{
		DrawCircle2D(*g_pThreadSynchronizer->GetAppFrameData().commandPoolToFill, pos.m_vec, radius, true);
	}

	static void localDrawCircleNormalizedScreenAlligned(const Vec2& pos, float radius)
	{
		DrawCircle2D(*g_pThreadSynchronizer->GetAppFrameData().commandPoolToFill, pos.m_vec, radius, false);
	}

	static void localDrawCircle(const Vec2& pos, float radius)
	{
		DrawCircle2DPixelSpace(*g_pThreadSynchronizer->GetAppFrameData().commandPoolToFill, pos.m_vec, radius, true);
	}

	static void localDrawCircleScreenAlligned(const Vec2& pos, float radius)
	{
		DrawCircle2DPixelSpace(*g_pThreadSynchronizer->GetAppFrameData().commandPoolToFill, pos.m_vec, radius, false);
	}
}


void Hail::AngelScript::Handler::RegisterGlobalMessages()
{
	// Print functions
	bool bResult;
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "Print", "void" }, { {"text", "string", true, true, true } }, asFUNCTION(localPrint), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "PrintError", "void" }, { {"text", "string", true, true, true } }, asFUNCTION(localPrintError), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "PrintWarning", "void" }, { {"text", "string", true, true, true } }, asFUNCTION(localPrintWarning), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");
	
	// Vector2
	RegisterAngelScriptVectorType();

	// Math
	RegisterScriptMath(m_pScriptEngine);

	// Input handling
	// Input Enum
	bResult = m_pScriptEngine->RegisterEnum("eInputAction");
	// Register the object properties
	H_ASSERT(bResult, "Failed to register input enum");
	for (int i = 0; i < (int)eInputAction::InputCount; i++)
	{
		bResult = m_pTypeRegistry->RegisterGlobalEnumValue("eInputAction", localGetInputActionMapValueName((eInputAction)i), i, H_FILE_LINE);
		H_ASSERT(bResult, "Failed to register enum value");
		//r = m_pScriptEngine->RegisterEnumValue("eInputAction", localGetInputActionMapValueName((eInputAction)i), i);
	}
	// Input handler
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "GetDirectionInput", "Vec2" }, { {"action", "eInputAction", }, { "gamepadToCheck", "int" } }, asFUNCTION(localGetDirectionInput), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "GetButtonInput", "int" }, { {"action", "eInputAction", }, { "gamepadToCheck", "int" } }, asFUNCTION(localGetButtonInput), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "GetGamepadTriggerInput", "float" }, { {"action", "eInputAction", }, { "gamepadToCheck", "int" } }, asFUNCTION(localGetGamepadTriggerInput), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");

	// Debug lines

	const VectorOnStack<VariableTypeData, 8u> lineArgumentVariables = { {"start", "Vec2", true, true, true }, { "end", "Vec2", true, true, true } };
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "DrawLineNormalized", "void" }, lineArgumentVariables, asFUNCTION(localDrawLineNormalized), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "DrawLineNormalizedScreenAlligned", "void" }, lineArgumentVariables, asFUNCTION(localDrawLineNormalizedScreenAlligned), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "DrawLine", "void" }, lineArgumentVariables, asFUNCTION(localDrawLine), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "DrawLineScreenAlligned", "void" },	lineArgumentVariables,			asFUNCTION(localDrawLineScreenAlligned), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");

	const VectorOnStack<VariableTypeData, 8u> lineRotLengthArgumentVariables = { {"start", "Vec2", true, true, true }, { "rotationRad", "float" }, { "length", "float" }};
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "DrawLineNormalized", "void" }, lineRotLengthArgumentVariables, asFUNCTION(localDrawLineNormalizedRotLength), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "DrawLineNormalizedScreenAlligned", "void" }, lineRotLengthArgumentVariables, asFUNCTION(localDrawLineNormalizedRotLengthScreenAlligned), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "DrawLine", "void" }, lineRotLengthArgumentVariables, asFUNCTION(localDrawLineRotLength), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "DrawLineScreenAlligned", "void" },			lineRotLengthArgumentVariables, asFUNCTION(localDrawLineRotLengthScreenAlligned), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");

	const VectorOnStack<VariableTypeData, 8u> drawBoxArgumentVariables = { {"pos", "Vec2", true, true, true }, { "dimensions", "Vec2", true, true, true } };
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "DrawBoxNormalized", "void" }, drawBoxArgumentVariables, asFUNCTION(localDrawBoxNormalized), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "DrawBoxNormalizedScreenAlligned", "void" }, drawBoxArgumentVariables, asFUNCTION(localDrawBoxNormalizedScreenAlligned), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "DrawBox", "void" }, drawBoxArgumentVariables, asFUNCTION(localDrawBox), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "DrawBoxScreenAlligned", "void" }, drawBoxArgumentVariables, asFUNCTION(localDrawBoxScreenAlligned), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");

	const VectorOnStack<VariableTypeData, 8u> drawBoxMinMaxArgumentVariables = { {"min", "Vec2", true, true, true }, { "max", "Vec2", true, true, true } };
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "DrawBoxMinMaxNormalized", "void" }, drawBoxMinMaxArgumentVariables, asFUNCTION(localDrawBoxMinMaxNormalized), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "DrawBoxMinMaxNormalizedScreenAlligned", "void" }, drawBoxMinMaxArgumentVariables, asFUNCTION(localDrawBoxMinMaxNormalizedScreenAlligned), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "DrawBoxMinMax", "void" }, drawBoxMinMaxArgumentVariables, asFUNCTION(localDrawBoxMinMax), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "DrawBoxMinMaxScreenAlligned", "void" }, drawBoxMinMaxArgumentVariables, asFUNCTION(localDrawBoxMinMaxScreenAlligned), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");

	const VectorOnStack<VariableTypeData, 8u> drawCircleArgumentVariables = { {"pos", "Vec2", true, true, true }, { "radius", "float" } };
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "DrawCircleNormalized", "void" }, drawCircleArgumentVariables, asFUNCTION(localDrawCircleNormalized), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "DrawCircleNormalizedScreenAlligned", "void" }, drawCircleArgumentVariables, asFUNCTION(localDrawCircleNormalizedScreenAlligned), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "DrawCircle", "void" }, drawCircleArgumentVariables, asFUNCTION(localDrawCircle), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");
	bResult = m_pTypeRegistry->RegisterGlobalMethod({ "DrawCircleScreenAlligned", "void" }, drawCircleArgumentVariables, asFUNCTION(localDrawCircleScreenAlligned), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register global func");
}

//-----------------------
// AngelScript functions
//-----------------------

static Variable GetVec2VariableData(void* pObj)
{
	Vec2 vector = *(Vec2*)pObj;
	Variable variableToReturn;
	variableToReturn.m_type = "Vec2";
	variableToReturn.m_value = StringL::Format("x : %f, y : %f", vector.GetX(), vector.GetY());

	Variable& memberX = variableToReturn.m_members.Add();
	memberX.m_name = "x";
	memberX.m_type = "float";
	memberX.m_value = StringL::Format("%f", vector.GetX());
	Variable& memberY = variableToReturn.m_members.Add();
	memberY.m_name = "y";
	memberY.m_type = "float";
	memberY.m_value = StringL::Format("%f", vector.GetY());

	return variableToReturn;
}

static void Vec2DefaultConstructor(Vec2* self)
{
	new(self) Vec2();
}

void Vec2DefaultDestructor(Vec2* /*thisPtr*/)
{
}

static void Vec2CopyConstructor(const Vec2& other, Vec2* self)
{
	new(self) Vec2(other);
}

static void Vec2ConvConstructor(float val, Vec2* self)
{
	new(self) Vec2(val);
}

static void Vec2InitConstructor(float x, float y, Vec2* self)
{
	new(self) Vec2(x, y);
}

static void Vec2ListConstructor(float* list, Vec2* self)
{
	new(self) Vec2(list[0], list[1]);
}


void Hail::AngelScript::Handler::RegisterAngelScriptVectorType()
{
	int r;
	// Register a primitive type, that doesn't need any special management of the content

	// Register the type
	
	r = m_pTypeRegistry->RegisterType("Vec2", sizeof(Vec2), asOBJ_VALUE | asOBJ_APP_CLASS_CAK | asOBJ_APP_CLASS_ALLFLOATS, H_FILE_LINE); assert(r >= 0);
	r = m_pTypeRegistry->RegisterVariableFunction("Vec2", &GetVec2VariableData); assert(r >= 0);
	bool bResult;

	// Register the constructors
	bResult = m_pTypeRegistry->RegisterClassConstructor("Vec2", {}, {}, asFUNCTION(Vec2DefaultConstructor), true, H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassConstructor("Vec2", {}, {}, asFUNCTION(Vec2DefaultDestructor), false, H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassConstructor("Vec2", { { "in", "Vec2", true, true } }, {}, asFUNCTION(Vec2CopyConstructor), true, H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassConstructor("Vec2", { { "", "float" } }, {}, asFUNCTION(Vec2ConvConstructor), true, H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassConstructor("Vec2", { { "", "float" }, { "", "float" } }, {}, asFUNCTION(Vec2InitConstructor), true, H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassConstructor("Vec2", { { "in", "int", true, true } }, { { "", "float" }, { "", "float" } }, asFUNCTION(Vec2ListConstructor), true, H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");

	// Register the object properties
	bResult = m_pTypeRegistry->RegisterClassObjectMember("Vec2", { "x", "float" }, asOFFSET(Vec2, m_vec.x), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassObjectMember("Vec2", { "y", "float" }, asOFFSET(Vec2, m_vec.y), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");

	// Register the operator overloads
	bResult = m_pTypeRegistry->RegisterClassOperatorOverload("Vec2", { "opAssign", "Vec2", false, true }, asMETHODPR(Vec2, operator=, (const Vec2&), Vec2&), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassOperatorOverload("Vec2", { "opAddAssign", "Vec2", false, true }, asMETHODPR(Vec2, operator+=, (const Vec2&), Vec2&), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassOperatorOverload("Vec2", { "opSubAssign", "Vec2", false, true }, asMETHODPR(Vec2, operator-=, (const Vec2&), Vec2&), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassOperatorOverload("Vec2", { "opMulAssign", "Vec2", false, true }, asMETHODPR(Vec2, operator*=, (const Vec2&), Vec2&), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassOperatorOverload("Vec2", { "opDivAssign", "Vec2", false, true }, asMETHODPR(Vec2, operator/=, (const Vec2&), Vec2&), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassOperatorOverload("Vec2", { "opEquals", "bool", true, false }, asMETHODPR(Vec2, operator==, (const Vec2&)const, bool), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassOperatorOverload("Vec2", { "opAdd", "Vec2", true, false }, asMETHODPR(Vec2, operator+, (const Vec2&) const, Vec2), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassOperatorOverload("Vec2", { "opSub", "Vec2", true, false }, asMETHODPR(Vec2, operator-, (const Vec2&) const, Vec2), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassOperatorOverload("Vec2", { "opMul", "Vec2", true, false }, asMETHODPR(Vec2, operator*, (const Vec2&) const, Vec2), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassOperatorOverload("Vec2", { "opDiv", "Vec2", true, false }, asMETHODPR(Vec2, operator/, (const Vec2&) const, Vec2), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");

	// Register the object methods
	bResult = m_pTypeRegistry->RegisterClassMethod("Vec2", { "Length", "float", true }, {}, asMETHOD(Vec2, Length), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassMethod("Vec2", { "SquaredLength", "float", true }, {}, asMETHOD(Vec2, SquaredLength), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassMethod("Vec2", { "GetNormalized", "Vec2", true }, {}, asMETHOD(Vec2, GetNormalized), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassMethod("Vec2", { "Normalize", "void" }, {}, asMETHOD(Vec2, Normalize), H_FILE_LINE);  H_ASSERT(bResult, "Failed to register Vec2 func");

	// Register the swizzle operators
	bResult = m_pTypeRegistry->RegisterClassGetSetter("Vec2", { "yx", "Vec2" }, asMETHOD(Vec2, YX), false, H_FILE_LINE); H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassGetSetter("Vec2", { "xx", "Vec2" }, asMETHOD(Vec2, XX), false, H_FILE_LINE); H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassGetSetter("Vec2", { "yy", "Vec2" }, asMETHOD(Vec2, YY), false, H_FILE_LINE); H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassGetSetter("Vec2", { "x", "float" }, asMETHOD(Vec2, GetX), false, H_FILE_LINE); H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassGetSetter("Vec2", { "y", "float" }, asMETHOD(Vec2, GetY), false, H_FILE_LINE); H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassGetSetter("Vec2", { "x", "float" }, asMETHOD(Vec2, SetX), true, H_FILE_LINE); H_ASSERT(bResult, "Failed to register Vec2 func");
	bResult = m_pTypeRegistry->RegisterClassGetSetter("Vec2", { "y", "float" }, asMETHOD(Vec2, SetY), true, H_FILE_LINE); H_ASSERT(bResult, "Failed to register Vec2 func");
}

Hail::AngelScript::Vec2::Vec2() : m_vec(0) {}

Hail::AngelScript::Vec2::Vec2(glm::vec2 vec) : m_vec(vec) {}

Hail::AngelScript::Vec2::Vec2(const Vec2& other) : m_vec(other.m_vec) {}

Hail::AngelScript::Vec2::Vec2(float val) : m_vec(val, val) {}

Hail::AngelScript::Vec2::Vec2(float x, float y) : m_vec(x, y) {}

Vec2& Hail::AngelScript::Vec2::operator=(const Vec2& other)
{
	m_vec = other.m_vec;
	return *this;
}

Vec2& Hail::AngelScript::Vec2::operator+=(const Vec2& other)
{
	m_vec += other.m_vec;
	return *this;
}

Vec2& Hail::AngelScript::Vec2::operator-=(const Vec2& other)
{
	m_vec -= other.m_vec;
	return *this;
}

Vec2& Hail::AngelScript::Vec2::operator*=(const Vec2& other)
{
	m_vec *= other.m_vec;
	return *this;
}

Vec2& Hail::AngelScript::Vec2::operator/=(const Vec2& other)
{
	m_vec /= other.m_vec;
	return *this;
}

float Hail::AngelScript::Vec2::Length() const
{
	return m_vec.length();
}

float Hail::AngelScript::Vec2::SquaredLength() const
{
	return m_vec.x * m_vec.x + m_vec.y * m_vec.y;
}

Vec2 Hail::AngelScript::Vec2::GetNormalized() const
{
	Vec2 returnVector{};
	if (m_vec.x != 0.0 && m_vec.y != 0.0)
	{
		const glm::vec2 vec2 = glm::normalize(m_vec);
		returnVector = Vec2(vec2.x, vec2.y);
	}
	return returnVector;
}

void Hail::AngelScript::Vec2::Normalize()
{
	if (m_vec.x != 0.0 || m_vec.y != 0.0)
	{
		m_vec = glm::normalize(m_vec);
	}
}

bool Hail::AngelScript::Vec2::operator==(const Vec2& other) const
{
	return m_vec == other.m_vec;
}

bool Hail::AngelScript::Vec2::operator!=(const Vec2& other) const
{
	return m_vec != other.m_vec;
}

Vec2 Hail::AngelScript::Vec2::YX() const
{
	return Vec2(m_vec.y, m_vec.x);
}

Vec2 Hail::AngelScript::Vec2::XX() const
{
	return Vec2(m_vec.x, m_vec.x);
}

Vec2 Hail::AngelScript::Vec2::YY() const
{
	return Vec2(m_vec.y, m_vec.y);
}

void Hail::AngelScript::Vec2::SetX(float x)
{
	m_vec.x = x;
}

void Hail::AngelScript::Vec2::SetY(float y)
{
	m_vec.y = y;
}

float Hail::AngelScript::Vec2::GetX() const
{
	return m_vec.x;
}

float Hail::AngelScript::Vec2::GetY() const
{
	return m_vec.y;
}

Vec2 Hail::AngelScript::Vec2::operator+(const Vec2& other) const
{
	const glm::vec2 vec = m_vec + other.m_vec;
	return Vec2(vec.x, vec.y);
}

Vec2 Hail::AngelScript::Vec2::operator-(const Vec2& other) const
{
	const glm::vec2 vec = m_vec - other.m_vec;
	return Vec2(vec.x, vec.y);
}

Vec2 Hail::AngelScript::Vec2::operator*(const Vec2& other) const
{
	const glm::vec2 vec = m_vec * other.m_vec;
	return Vec2(vec.x, vec.y);
}

Vec2 Hail::AngelScript::Vec2::operator/(const Vec2& other) const
{
	const glm::vec2 vec = m_vec / other.m_vec;
	return Vec2(vec.x, vec.y);
}

Hail::AngelScript::Handler::Handler(InputActionMap* pInputActionMap, ThreadSyncronizer* pThreadSyncronizer, bool bEnableDebugger) : m_bEnableDebugger(bEnableDebugger)
{
	H_ASSERT(pInputActionMap, "Must set action input map with a valid pointer.");
	H_ASSERT(pThreadSyncronizer, "Must set thread synchronizer with a valid pointer.");
	g_pInputActionMap = pInputActionMap;
	g_pThreadSynchronizer = pThreadSyncronizer;

	m_pScriptEngine = asCreateScriptEngine();
	H_ASSERT(m_pScriptEngine, "Failed to create angelscript engine.");
	m_errorHandler.Init(m_pScriptEngine, m_bEnableDebugger);
	m_pTypeRegistry = new TypeRegistry(m_pScriptEngine, m_bEnableDebugger);
	RegisterScriptArray(m_pTypeRegistry, true);
	RegisterStdString(m_pScriptEngine, m_pTypeRegistry);
	RegisterGlobalMessages();
}

void Hail::AngelScript::Handler::SetActiveScriptRunner(Runner* pRunner)
{
	m_errorHandler.SetActiveScriptRunner(pRunner);
}

