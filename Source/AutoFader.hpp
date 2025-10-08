#pragma once

#include <functional>
#include <optional>

#include "MathCPP/Duration.hpp"

#include "Settings.hpp"

using namespace MathsCPP;

class AutoFader {
public:
	void OnLoop(const Delta &time) {
		if (targetAlpha) {
			if (alpha < *targetAlpha) {
				alpha += static_cast<float>(time.change.AsSeconds() * Settings::settings.GetAutoFadeSpeed());
				if (alpha >= *targetAlpha) {
					alpha = *targetAlpha;
					targetAlpha = std::nullopt;
				}
			} else if (alpha > *targetAlpha) {
				alpha -= static_cast<float>(time.change.AsSeconds() * Settings::settings.GetAutoFadeSpeed());
				if (alpha <= *targetAlpha) {
					alpha = *targetAlpha;
					targetAlpha = std::nullopt;
				}
			}
		}

		if (auto now = std::chrono::system_clock::now(); (now - lastEventTime) > waitTime) {
			if (Settings::settings.GetAutoFade()) 
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

		if (fadeCallback && (!lastFade || *lastFade != in)) {
			fadeCallback(in);
			lastFade = in;
		}
	}

	void SetWaitTime(Duration<Microseconds> waitTime) {
		this->waitTime = waitTime;
	}

	void SetFadeCallback(std::function<void(bool)> &&callback) { 
		this->fadeCallback = std::move(callback); 
	}

	const float &GetAlpha() const { return alpha; }

protected:
	std::chrono::seconds waitTime = Settings::settings.GetWaitTime();

	float alpha = 1.0f;
	std::optional<float> targetAlpha = std::nullopt;

	std::chrono::system_clock::time_point lastEventTime = std::chrono::system_clock::now();

	std::function<void(bool)> fadeCallback;

	std::optional<bool> lastFade = std::nullopt;
};