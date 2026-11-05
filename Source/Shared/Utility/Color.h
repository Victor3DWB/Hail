#pragma once

#include "Types.h"
#include "glm\glm.hpp"

namespace Hail
{
	class Color
	{
	public:
		Color();
		Color(glm::vec3 color, float alpha);
		Color(glm::vec3 color);
		Color(glm::vec4 colorAndAlpha);
		Color(uint32 packedColor);

		Color& operator=(uint32 color);
		Color& operator=(glm::vec3 color);
		Color& operator=(glm::vec4 colorAndAlpha);

		uint32 GetColorPacked() const;
		glm::vec3 GetColorVec3() const;
		glm::vec4 GetColorVec4() const;

		void SetR(float colorValue);
		void SetG(float colorValue);
		void SetB(float colorValue);
		void SetA(float colorValue);

		static Color Lerp(const Color& color1, const Color& color2, float T);

		static const Color
			White,
			Black,
			Red,
			Green,
			Blue,
			Orange,
			Yellow,
			LimeGreen,
			Cyan,
			MarineBlue,
			PurpleBlue,
			Magenta;

	private:
		uint32 m_colorValue;
	};

}