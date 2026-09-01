#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <optional>

#include "MathCPP/Colour.hpp"
#include "MathCPP/Duration.hpp"
#include "OpenGL/Context.hpp"
#include "DynamicGain.hpp"

using namespace Fetcko;

class AlbumArt;
class Renderer {
public:
	Renderer(
		const DynamicGain<float> *dynamicGain,
		const AlbumArt *const albumArt
	);

	Renderer(Renderer &&other) noexcept;

	virtual ~Renderer();

	virtual bool IsFloatingPoint() const = 0;

	virtual void OnInit(
		int windowWidth,
		int windowHeight
	);

	virtual void OnDestroy() = 0;

	virtual void OnResize(
		int windowWidth,
		int windowHeight,
		int maxDimension
	);

	virtual void SetBufferLength(std::size_t bufferLength, bool changed);

	// This buffer is always guaranteed to be the larger of the two
	virtual bool SetBuffer(const uint8_t *const buffer, std::size_t len, bool force = false);

	virtual void OnLoop(
		const Delta &time,
		bool fileLoaded,
		float hStep,
		Context &context,
		const Colour<float> &color,
		const Colour<float> &brightColor,
		float frameCount,
		float maxHeartSample = 0.0f,
		bool resetGain = false,
		bool miniPlayer = false
	) = 0;

	virtual void Draw(
		const Delta &time,
		float frameCount,
		const Colour<float> &color,
		const Vector<int, 2> &blurOffset,
		Context &context
	) = 0;
	virtual void Reset() = 0;

	virtual void SetNumberOfChannels(uint8_t numberOfChannels);

	const bool GetPulses() const;
	const bool GetPulse() const;

	void TogglePulse();
	void SetPulse(bool pulse);
	void SetPulseTime(Duration<Microseconds> time);

	void SetScale(float scale);
	void SetOffset(int offset);

protected:
	void SetColor(const Colour<float> &color, float alpha, Context &context);

	float GetThickness(bool miniPlayer) const;
	inline float GetEffectOffset() const;
	float GetHeight(float margin, bool miniPlayer) const;

	bool initialized = false;

	int windowWidth = 0, windowHeight = 0, maxDimension = 0;
	std::size_t fullBufferLength = 0;
	std::size_t bufferLength = 0;

	const DynamicGain<float> *dynamicGain = nullptr;
	const AlbumArt * const albumArt = nullptr;

	const uint8_t *buffer = nullptr;

	uint8_t numberOfChannels = 2;

	bool pulses = false;
	bool pulse = false;
	Duration<Microseconds> pulseTime = 0.1s;

	float scale = 1.0f;

	// Set in the constructor
	int offset;
};

class RendererFactory {
public:
	static void Register(
		std::string renderer,
		std::function<Renderer *(
			const DynamicGain<float> *,
			const AlbumArt *,
			Renderer *,
			std::optional<int>,
			std::optional<int>,
			uint8_t *,
			std::optional<std::size_t>,
			std::optional<std::size_t>
		)> f
	) {
		builders.emplace(std::make_pair(renderer, f));
	}
	static Renderer *Build(
		std::string renderer,
		const DynamicGain<float> *dynamicGain,
		const AlbumArt *albumArt,
		Renderer *oldRenderer = nullptr,
		std::optional<int> windowWidth = std::nullopt,
		std::optional<int> windowHeight = std::nullopt,
		uint8_t *buffer = nullptr,
		std::optional<std::size_t> maxLength = std::nullopt,
		std::optional<std::size_t> bufferLength = std::nullopt
	) {
		return builders.at(renderer)(
			dynamicGain,
			albumArt,
			oldRenderer,
			windowWidth,
			windowHeight,
			buffer,
			maxLength,
			bufferLength
		);
	}

private:
	static inline std::map<
		std::string, 
		std::function<
			Renderer *(
				const DynamicGain<float> *,
				const AlbumArt *,
				Renderer *,
				std::optional<int>,
				std::optional<int>,
				uint8_t *,
				std::optional<std::size_t>,
				std::optional<std::size_t>
			)
		>
	> builders;
};