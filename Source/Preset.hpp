#pragma once

#include <vector>

#include "MathCPP/Duration.hpp"
#include "Serial/Json.hpp"
#include "Utils/Logger.hpp"

using namespace Fetcko;
using namespace MathsCPP;
using namespace serial;

class Preset {
public:
	Preset() = default;

	Preset(
		const std::string &name,
		std::size_t bufferSize,
		Duration<Microseconds> decayTime,
		Duration<Microseconds> fadeTime,
		bool pulse,
		Duration<Microseconds> pulseTime,
		bool strobe,
		float strobeIntensity,
		bool rotating,
		float rotationSpeed,
		bool blur,
		float blurIntensity,
		float blurOpacity,
		const std::string &effect,
		float effectIntensity,
		float effectXOffset,
		float effectYOffset,
		float effectRadiation,
		float effectHorizontalSpread,
		float effectVerticalSpread,
		float effectRotation,
		std::optional<std::string> renderer = std::nullopt,
		std::optional<float> scale = std::nullopt,
		std::optional<int> rendererOffset = std::nullopt
	) : name(name),
		bufferSize(bufferSize),
		decayTime(decayTime),
		fadeDecayTime(fadeTime),
		pulse(pulse),
		pulseTime(pulseTime),
		strobe(strobe),
		strobeIntensity(strobeIntensity),
		rotating(rotating),
		rotationSpeed(rotationSpeed),
		blur(blur),
		blurIntensity(blurIntensity),
		blurOpacity(blurOpacity),
		effect(effect),
		effectIntensity(effectIntensity),
		effectXOffset(effectXOffset),
		effectYOffset(effectYOffset),
		effectRadiation(effectRadiation),
		effectHorizontalSpread(effectHorizontalSpread),
		effectVerticalSpread(effectVerticalSpread),
		effectRotation(effectRotation),
		renderer(renderer),
		scale(scale),
		rendererOffset(rendererOffset) {

	}

	static Preset Random();

	static const std::vector<Preset> &GetPresets() { return Presets; }
	static void AddPreset(Preset &&preset);
	static void RemovePreset(std::size_t index);

	const std::string &GetName() const { return name; }
	const std::size_t &GetBufferSize() const { return bufferSize; }
	const Duration<Microseconds> &GetDecayTime() const { return decayTime; }
	const Duration<Microseconds> &GetFadeTime() const { return fadeDecayTime; }
	const bool GetPulse() const { return pulse; }
	const Duration<Microseconds> &GetPulseTime() const { return pulseTime; }
	const bool GetStrobe() const { return strobe; }
	const float GetStrobeIntensity() const { return strobeIntensity; }
	const std::optional<bool> &GetRotating() const { return rotating; }
	const float GetRotationSpeed() const { return rotationSpeed; }
	const std::optional<bool> &GetBlur() const { return blur; }
	const float GetBlurIntensity() const { return blurIntensity; }
	const float GetBlurOpacity() const { return blurOpacity; }
	const std::string &GetEffect() const { return effect; }
	const float GetEffectIntensity() const { return effectIntensity; }
	const float GetEffectXOffset() const { return effectXOffset; }
	const float GetEffectYOffset() const { return effectYOffset; }
	const float GetEffectRadiation() const { return effectRadiation; }
	const float GetEffectHorizontalSpread() const { return effectHorizontalSpread; }
	const float GetEffectVerticalSpread() const { return effectVerticalSpread; }
	const float GetEffectRotation() const { return effectRotation; }
	const std::optional<std::string> &GetRenderer() const { return renderer; }
	const std::optional<float> &GetScale() const { return scale; }
	const std::optional<int> &GetRendererOffset() const { return rendererOffset; }

	friend const Node &operator>>(const Node &node, Preset &preset);
	friend Node &operator<<(Node &node, const Preset &preset);

private:
	static std::vector<Preset> Load();
	static void Save();
	static std::vector<Preset> Presets;

	std::string name;

	std::size_t bufferSize = 2048;

	Duration<Microseconds> fadeDecayTime = 0.5s;
	Duration<Microseconds> decayTime = 0.5s;

	bool pulse = false;
	Duration<Microseconds> pulseTime = 0.1s;

	bool strobe = false;
	float strobeIntensity = 0.66f;

	std::optional<bool> rotating = std::nullopt;
	float rotationSpeed = 1.0f;

	std::optional<bool> blur = std::nullopt;
	float blurIntensity = 0.5;
	float blurOpacity = 1.0f;

	std::string effect = "noeffect";
	float effectIntensity = 1.0f;
	float effectXOffset = 0.0f;
	float effectYOffset = 0.0f;
	float effectRadiation = 0.0f;
	float effectHorizontalSpread = 0.0f;
	float effectVerticalSpread = 0.0f;
	float effectRotation = 0.0f;

	std::optional<std::string> renderer = std::nullopt;
	std::optional<float> scale = std::nullopt;
	std::optional<int> rendererOffset = std::nullopt;
};