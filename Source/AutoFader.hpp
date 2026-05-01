#pragma once

#include <functional>
#include <optional>

#include "MathCPP/Duration.hpp"

#include "Settings.hpp"

using namespace MathsCPP;

template<bool UserControlled>
class AutoFader {
public:
	void OnLoop(const Delta &time) {
		if (targetAlpha) {
			if (alpha < *targetAlpha) {
				alpha += static_cast<float>(time.change.AsSeconds() * autoFadeSpeed);
				if (alpha >= *targetAlpha) {
					alpha = *targetAlpha;
					targetAlpha = std::nullopt;
				}
			} else if (alpha > *targetAlpha) {
				alpha -= static_cast<float>(time.change.AsSeconds() * autoFadeSpeed);
				if (alpha <= *targetAlpha) {
					alpha = *targetAlpha;
					targetAlpha = std::nullopt;
				}
			} else targetAlpha = std::nullopt;
		} else if (auto now = std::chrono::system_clock::now(); !paused && (now - lastEventTime) > waitTime) {
			if constexpr (UserControlled) {
				if (Settings::settings.GetAutoFade())
					Fade(false);
			} else {
				Fade(false);
			}

			lastEventTime = now;
		}
	}

	void Fade(bool in) {
		if (in) lastEventTime = std::chrono::system_clock::now();

		if (!lastFade || *lastFade != in) {
			if (in)
				targetAlpha = 1.0f;
			else
				targetAlpha = 0.0f;

			if (fadeCallback)
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

	void SetAutoFadeSpeed(float autoFadeSpeed) { this->autoFadeSpeed = autoFadeSpeed; }

	virtual const float &GetAlpha() const { return alpha; }

	void Stick() { paused = true; }
	void Unstick() { paused = false; }

protected:
	std::chrono::seconds waitTime = Settings::settings.GetWaitTime();

	float alpha = 0.0f;
	std::optional<float> targetAlpha = std::nullopt;

	std::chrono::system_clock::time_point lastEventTime = std::chrono::system_clock::now();

	std::function<void(bool)> fadeCallback;

	std::optional<bool> lastFade = std::nullopt;

	float autoFadeSpeed = Settings::settings.GetAutoFadeSpeed();

	bool paused = false;
};