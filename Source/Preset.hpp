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
		bool rotating,
		float rotationSpeed,
		bool blur,
		float blurIntensity
	) : name(name),
		bufferSize(bufferSize),
		decayTime(decayTime),
		fadeDecayTime(fadeTime),
		pulse(pulse),
		pulseTime(pulseTime),
		rotating(rotating),
		rotationSpeed(rotationSpeed),
		blur(blur),
		blurIntensity(blurIntensity) {

	}

	static const std::vector<Preset> &GetPresets() { return Presets; }
	static void AddPreset(Preset &&preset);
	static void RemovePreset(std::size_t index);

	const std::string &GetName() const { return name; }
	const std::size_t &GetBufferSize() const { return bufferSize; }
	const Duration<Microseconds> &GetDecayTime() const { return decayTime; }
	const Duration<Microseconds> &GetFadeTime() const { return fadeDecayTime; }
	const bool GetPulse() const { return pulse; }
	const Duration<Microseconds> &GetPulseTime() const { return pulseTime; }
	const std::optional<bool> &GetRotating() const { return rotating; }
	const float GetRotationSpeed() const { return rotationSpeed; }
	const std::optional<bool> &GetBlur() const { return blur; }
	const float GetBlurIntensity() const { return blurIntensity; }

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

	std::optional<bool> rotating = std::nullopt;
	float rotationSpeed = 1.0f;

	std::optional<bool> blur = std::nullopt;
	float blurIntensity = 0.5;
};