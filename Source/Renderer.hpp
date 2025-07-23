#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <optional>

#include "MathCPP/Duration.hpp"

#include "AlbumArt.hpp"
#include "DynamicGain.hpp"

class Renderer {
public:
	Renderer(
		const DynamicGain<float> *dynamicGain,
		const AlbumArt * const albumArt
	) : albumArt(albumArt) {
		this->dynamicGain = dynamicGain;
	}

	virtual ~Renderer() {

	}

	virtual bool IsFloatingPoint() const = 0;

	virtual void OnInit(
		int windowWidth,
		int windowHeight
	) {
		this->windowWidth = windowWidth;
		this->windowHeight = windowHeight;

		initialized = true;
	}

	virtual void OnResize(
		int windowWidth,
		int windowHeight
	) {
		this->windowWidth = windowWidth;
		this->windowHeight = windowHeight;
	}

	virtual void SetBufferLength(std::size_t bufferLength, bool changed) {
		this->bufferLength = bufferLength;
	}

	// This buffer is always guaranteed to be the larger of the two
	virtual bool SetBuffer(const uint8_t *const buffer, std::size_t len, bool force = false) {
		this->buffer = buffer;
		if (len != fullBufferLength) {
			fullBufferLength = len;
			return true;
		}
		return force;
	}

	virtual void OnLoop(
		const Delta &time,
		bool fileLoaded,
		float hStep,
		float maxHeartSample = 0.0f,
		bool resetGain = false
	) = 0;

	virtual void Draw(
		const Delta &time,
		float frameCount,
		const Colour<float> &color
	) = 0;
	virtual void Reset() = 0;

	void SetNumberOfChannels(uint8_t numberOfChannels) {
		this->numberOfChannels = numberOfChannels;
	}

	void TogglePulse() { pulse = !pulse; }
	void SetPulse(bool pulse) { this->pulse = pulse; }
	void SetPulseTime(Duration<Microseconds> time) {
		pulseTime = time;
	}

protected:
	void SetColor(const Colour<float> &color, float alpha) {
		glColor4f(color.r, color.g, color.b, alpha);
	}

	bool initialized = false;

	int windowWidth = 0, windowHeight = 0;
	std::size_t fullBufferLength = 0;
	std::size_t bufferLength = 0;

	const DynamicGain<float> *dynamicGain = nullptr;
	const AlbumArt * const albumArt = nullptr;

	const uint8_t *buffer = nullptr;

	uint8_t numberOfChannels = 2;

	bool pulse = false;
	Duration<Microseconds> pulseTime = 0.1s;
};