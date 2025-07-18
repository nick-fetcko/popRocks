#pragma once

#include "MathCPP/Colour.hpp"

class ColorChangeListener {
public:
	virtual ~ColorChangeListener() = default;
	virtual void OnColorChanged(const MathsCPP::Colour<float> &color, bool silent = false) = 0;
};