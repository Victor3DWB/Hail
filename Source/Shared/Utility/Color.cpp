#include "Shared_PCH.h"
#include "Color.h"
#include "glm\common.hpp"

using namespace Hail;

namespace
{
	uint32 locPackColorVec3(glm::vec3 colorToConvert)
	{
		// Drag down values above 1.0
		colorToConvert = glm::min(colorToConvert, 1.0f);

		return uint32(uint8(colorToConvert.x * 255.f) << 24 | uint8(colorToConvert.y * 255.f) << 16 | uint8(colorToConvert.z * 255.f) << 8);
	}
	uint32 locPackColorVec4(glm::vec4 colorToConvert)
	{
		// Drag down values above 1.0
		colorToConvert = glm::min(colorToConvert, 1.0f);

		return uint32(uint8(colorToConvert.x * 255.f) << 24 | uint8(colorToConvert.y * 255.f) << 16 | uint8(colorToConvert.z * 255.f) << 8 | uint8(colorToConvert.w * 255.f));
	}

	glm::vec3 locUnPackColorVec3(uint32 packedColor)
	{
		glm::vec3 returnColor;
		returnColor.x = float((packedColor >> 24) & 0xff) / 255.0;
		returnColor.y = float((packedColor >> 16) & 0xff) / 255.0;
		returnColor.z = float((packedColor >> 8) & 0xff) / 255.0;
		return returnColor;
	}
	glm::vec4 locUnPackColorVec4(uint32 packedColor)
	{
		glm::vec4 returnColor;
		returnColor.x = float((packedColor >> 24) & 0xff) / 255.0;
		returnColor.y = float((packedColor >> 16) & 0xff) / 255.0;
		returnColor.z = float((packedColor >> 8) & 0xff) / 255.0;
		returnColor.w = float((packedColor) & 0xff) / 255.0;
		return returnColor;
	}
}

Hail::Color::Color() : m_colorValue(MAX_UINT)
{
}

Hail::Color::Color(glm::vec3 color)
{
	m_colorValue = locPackColorVec4({ color.x, color.y, color.z, 1.0f });
}

Hail::Color::Color(glm::vec3 color, float alpha)
{
	m_colorValue = locPackColorVec4({ color.x, color.y, color.z, alpha });
}

Hail::Color::Color(glm::vec4 colorAndAlpha)
{
	m_colorValue = locPackColorVec4(colorAndAlpha);
}

Hail::Color::Color(uint32 packedColor)
{
	m_colorValue = packedColor;
}

Color& Hail::Color::operator=(uint32 color)
{
	m_colorValue = color;
	return *this;
}

Color& Hail::Color::operator=(glm::vec3 color)
{
	m_colorValue = locPackColorVec4({ color.x, color.y, color.z, 1.0 });
	return *this;
}

Color& Hail::Color::operator=(glm::vec4 colorAndAlpha)
{
	m_colorValue = locPackColorVec4(colorAndAlpha);
	return *this;
}

uint32 Hail::Color::GetColorPacked() const
{
	return m_colorValue;
}

glm::vec3 Hail::Color::GetColorVec3() const
{
	return locUnPackColorVec3(m_colorValue);
}

glm::vec4 Hail::Color::GetColorVec4() const
{
	return locUnPackColorVec4(m_colorValue);
}

void Hail::Color::SetR(float colorValue)
{
	uint8 value = colorValue * 255.f;
	m_colorValue = (value << 24) | (m_colorValue & 0x00ffffff);
}

void Hail::Color::SetG(float colorValue)
{
	uint8 value = colorValue * 255.f;
	m_colorValue = (value << 16) | (m_colorValue & 0xff00ffff);
}

void Hail::Color::SetB(float colorValue)
{
	uint8 value = colorValue * 255.f;
	m_colorValue = (value << 8) | (m_colorValue & 0xffff00ff);
}

void Hail::Color::SetA(float colorValue)
{
	uint8 value = colorValue * 255.f;
	m_colorValue = value << 8 | (m_colorValue & 0xffffffff);
}

Color Hail::Color::Lerp(const Color& color1, const Color& color2, float T)
{
	glm::vec4 mixedColor = glm::mix(color1.GetColorVec4(), color2.GetColorVec4(), T);
	return Color(mixedColor);
}

const Color Color::White(MAX_UINT);
const Color Color::Black(0u);
const Color Color::Red(glm::vec3(1.0f, 0.0f, 0.0f));
const Color Color::Green(glm::vec3(0.0f, 1.0f, 0.0f));
const Color Color::Blue(glm::vec3(0.0f, 0.0f, 1.0f));
const Color Color::Orange(glm::vec3(1.0f, 0.5f, 0.0f));
const Color Color::Yellow(glm::vec3(1.0f, 0.9f, 0.0f));
const Color Color::LimeGreen(glm::vec3(0.5f, 1.0f, 0.0f));
const Color Color::Cyan(glm::vec3(0.0f, 1.0f, 0.5f));
const Color Color::MarineBlue(glm::vec3(0.0f, 0.5f, 1.0f));
const Color Color::PurpleBlue(glm::vec3(0.5f, 0.0f, 1.0f));
const Color Color::Magenta(glm::vec3(1.0f, 0.0f, 0.5f));