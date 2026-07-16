#pragma once
#include "glm/gtc/type_ptr.hpp"

struct UCharColor
{
	unsigned char red, green, blue, alpha;

	UCharColor operator*(float value) const {
		return {
			static_cast<unsigned char>(glm::clamp(red * value, 0.0f, 255.0f)),
			static_cast<unsigned char>(glm::clamp(green * value, 0.0f, 255.0f)),
			static_cast<unsigned char>(glm::clamp(blue * value, 0.0f, 255.0f))
		};
	}

	UCharColor operator/(float value) const {
		return (*this) * (1.0f / value);
	}
};

constexpr UCharColor palette[9]
{
    {255, 20,  20,  255},
    {255, 150, 20,  255},
    {255, 255, 20,  255},
    {150, 255, 80,  255},
    {20,  255, 130, 255},
    {20,  150, 255, 255},
    {20,  20,  255, 255},
    {150, 20,  255, 255},
    {240, 80,  150, 255},
};