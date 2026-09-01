#pragma once

#include "MathCPP/Colour.hpp"

using namespace MathsCPP;

class HDR {
public:
	static inline bool Capable = false;
	static inline bool Enabled = false;
	static inline float WhiteLevel = 1.0f;
	static inline float Headroom = 1.0f;

	static inline Colourf WhiteColor = { 1.0f, 1.0f, 1.0f };

	static void SetWhiteLevel(float whiteLevel) {
		auto max = WhiteLevel * Headroom;
		WhiteLevel = whiteLevel;
		WhiteColor = { WhiteLevel, WhiteLevel, WhiteLevel };
		Headroom = max / WhiteLevel;
	}
};