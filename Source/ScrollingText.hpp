#pragma once

#include <chrono>
#include <optional>

#include "MathCPP/Duration.hpp"

#include "Text.hpp"

using namespace MathsCPP;
using namespace std::chrono_literals;

class ScrollingText : public Text {
private:
	const inline static auto PauseTime = 1s;

public:
	constexpr static int BleedEdge = 20;

	void OnResize(int windowWidth, int windowHeight);

	void OnLoop(int x, int y, const Delta &time);

	bool SetText(const std::string &text, bool force = false, bool update = true);

	void SetMaxWidth(float maxWidth);

	// In pixels-per-second
	// Default: 20
	void SetSpeed(float speed);

private:
	inline void UpdateWidthDelta() {
		widthDelta = (bounds.width - font->GetOutlineRadius() * 2) - maxWidth;

		// If we don't have enough space
		// to fade out the edges at least
		// halfway, don't bother scrolling
		if (widthDelta < BleedEdge / 2)
			widthDelta = 0.0f;

		offset = 0.0f;

		pauseTimer = std::chrono::system_clock::now();
	}

	float maxWidth = 0.0f;

	float widthDelta = 0.0f;
	float offset = 0.0f;
	float speed = 20.0f;

	std::optional<std::chrono::system_clock::time_point> pauseTimer = std::nullopt;

	int windowHeight = 0;
};