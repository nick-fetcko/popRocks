#pragma once

#include <vector>

#include "MathCPP/Duration.hpp"
#include "Serial/Json.hpp"

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
		Duration<Microseconds> pulseTime
	) : name(name),
		bufferSize(bufferSize),
		decayTime(decayTime),
		fadeDecayTime(fadeTime),
		pulse(pulse),
		pulseTime(pulseTime) {

	}

	static const std::vector<Preset> &GetPresets() { return Presets; }

	const std::string &GetName() const { return name; }
	const std::size_t &GetBufferSize() const { return bufferSize; }
	const Duration<Microseconds> &GetDecayTime() const { return decayTime; }
	const Duration<Microseconds> &GetFadeTime() const { return fadeDecayTime; }
	const bool GetPulse() const { return pulse; }
	const Duration<Microseconds> &GetPulseTime() const { return pulseTime; }

	friend const Node &operator>>(const Node &node, Preset &preset);

private:
	static std::vector<Preset> Load();
	const static std::vector<Preset> Presets;

	std::string name;

	std::size_t bufferSize = 2048;

	Duration<Microseconds> fadeDecayTime = 0.5s;
	Duration<Microseconds> decayTime = 0.5s;

	bool pulse = false;
	Duration<Microseconds> pulseTime = 0.1s;
};