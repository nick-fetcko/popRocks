#pragma once

#include <chrono>

#include "MathCPP/Vector.hpp"

using namespace std::chrono_literals;

using namespace MathsCPP;

class DoubleClick {
private:
	const inline static auto MaxTimeDelta = 500ms;

public:
	bool OnClick(const Vector2i &mousePos) {
		const auto now = std::chrono::system_clock::now();
		if (mousePos == lastPos && now - lastClick <= MaxTimeDelta) {
			Reset();
			return true;
		}

		lastClick = now;
		lastPos = mousePos;

		return false;
	}

	void Reset() {
		lastClick = std::chrono::system_clock::time_point();
		lastPos = { 0, 0 };
	}

private:
	std::chrono::system_clock::time_point lastClick;

	Vector2i lastPos = { 0, 0 };
};