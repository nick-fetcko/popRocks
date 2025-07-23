#pragma once

#include <functional>
#include <optional>

#include "MathCPP/Duration.hpp"

using namespace MathsCPP;

class AutoFader {
public:
	void OnLoop(const Delta &time) {
		if (targetAlpha) {
			if (alpha < *targetAlpha) {
				alpha += static_cast<float>(time.change.AsSeconds() * 2.0);
				if (alpha >= *targetAlpha) {
					alpha = *targetAlpha;
					targetAlpha = std::nullopt;
				}
			} else if (alpha > *targetAlpha) {
				alpha -= static_cast<float>(time.change.AsSeconds() * 2.0);
				if (alpha <= *targetAlpha) {
					alpha = *targetAlpha;
					targetAlpha = std::nullopt;
				}
			}
		}

		if (auto now = std::chrono::system_clock::now(); (now - lastEventTime) > waitTime) {
			Fade(false);
			lastEventTime = now;
		}
	}

	void Fade(bool in) {
		if (in) {
			targetAlpha = 1.0f;
			lastEventTime = std::chrono::system_clock::now();
		} else {
			targetAlpha = 0.0f;
		}

		if (fadeCallback)
			fadeCallback(in);
	}

	void SetFadeCallback(std::function<void(bool)> &&callback) { 
		this->fadeCallback = std::move(callback); 
	}

protected:
	std::chrono::seconds waitTime = 2s;

	float alpha = 1.0f;
	std::optional<float> targetAlpha = std::nullopt;

	std::chrono::system_clock::time_point lastEventTime = std::chrono::system_clock::now();

	std::function<void(bool)> fadeCallback;
};