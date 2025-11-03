#pragma once

#include "MathCPP/Colour.hpp"

using namespace MathsCPP;

class HDR {
public:
	static inline bool Enabled = false;
	static inline float WhiteLevel = 1.0f;
	static inline float Headroom = 1.0f;
};