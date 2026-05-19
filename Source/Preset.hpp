#pragma once

#include <set>
#include <vector>

#include <glad/glad.h>

#include "MathCPP/Duration.hpp"
#include "Serial/Json.hpp"
#include "Utils/Logger.hpp"

#include "DynamicGain.hpp"

using namespace Fetcko;
using namespace MathsCPP;
using namespace serial;

class Preset {
friend class Settings;

public:
	class ChangeListener {
	public:
		virtual ~ChangeListener() = default;

		virtual void OnPresetsChanged(const std::vector<Preset> &presets) = 0;
	};

	Preset() = default;

	Preset(
		const std::string &name,
		std::size_t bufferSize,
		int fftSize,
		Duration<Microseconds> decayTime,
		Duration<Microseconds> fadeTime,
		bool pulse,
		bool pulseBackground,
		Duration<Microseconds> pulseTime,
		bool strobe,
		float strobeIntensity,
		bool rotating,
		float rotationSpeed,
		bool blur,
		GLenum sourceFactor,
		GLenum destFactor,
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
		const DynamicGain<float> &dynamicGain,
		std::optional<std::string> renderer = std::nullopt,
		std::optional<float> scale = std::nullopt,
		std::optional<int> rendererOffset = std::nullopt,
		bool availableInMiniPlayer = false,
		std::optional<GLenum> sourceAlphaFactor = std::nullopt,
		std::optional<GLenum> destAlphaFactor = std::nullopt
	) : name(name),
		bufferSize(bufferSize),
		fftSize(fftSize),
		decayTime(decayTime),
		fadeDecayTime(fadeTime),
		pulse(pulse),
		pulseBackground(pulseBackground),
		pulseTime(pulseTime),
		strobe(strobe),
		strobeIntensity(strobeIntensity),
		rotating(rotating),
		rotationSpeed(rotationSpeed),
		blur(blur),
		sourceFactor(sourceFactor),
		destFactor(destFactor),
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
		dynamicGain(dynamicGain),
		renderer(renderer),
		scale(scale),
		rendererOffset(rendererOffset),
		availableInMiniPlayer(availableInMiniPlayer) {
		if (sourceAlphaFactor)
			this->sourceAlphaFactor = *sourceAlphaFactor;
		else
			this->sourceAlphaFactor = sourceFactor;

		if (destAlphaFactor)
			this->destAlphaFactor = *destAlphaFactor;
		else
			this->destAlphaFactor = destFactor;
	}

	static Preset Random(const DynamicGain<float> &dynamicGain);

	static const std::vector<Preset> &GetPresets() { return Presets; }
	static void AddPreset(Preset &&preset);
	static void RemovePreset(std::size_t index);

	const std::string &GetName() const { return name; }
	const std::size_t &GetBufferSize() const { return bufferSize; }
	const Duration<Microseconds> &GetDecayTime() const { return decayTime; }
	const Duration<Microseconds> &GetFadeTime() const { return fadeDecayTime; }
	const bool GetPulse() const { return pulse; }
	const bool GetPulseBackground() const { return pulseBackground; }
	const Duration<Microseconds> &GetPulseTime() const { return pulseTime; }
	const bool GetStrobe() const { return strobe; }
	const float GetStrobeIntensity() const { return strobeIntensity; }
	const std::optional<bool> &GetRotating() const { return rotating; }
	const float GetRotationSpeed() const { return rotationSpeed; }
	const std::optional<bool> &GetBlur() const { return blur; }
	const GLenum &GetSourceFactor() const { return sourceFactor; }
	const GLenum &GetDestFactor() const { return destFactor; }
	const GLenum &GetSourceAlphaFactor() const { return sourceAlphaFactor; }
	const GLenum &GetDestAlphaFactor() const { return destAlphaFactor; }
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
	const int &GetFftSize() const { return fftSize; }
	const bool &GetAvailableInMiniPlayer() const { return availableInMiniPlayer; }
	const DynamicGain<float> &GetDynamicGain() const { return dynamicGain; }

	static void AddChangeListener(ChangeListener *listener);
	static void RemoveChangeListener(ChangeListener *listener);

	friend const Node &operator>>(const Node &node, Preset &preset);
	friend Node &operator<<(Node &node, const Preset &preset);

private:
	static std::vector<Preset> Load();
	static void Save();
	static std::vector<Preset> Presets;
	static std::set<ChangeListener *> ChangeListeners;

	std::string name;

	std::size_t bufferSize = 2048;

	Duration<Microseconds> fadeDecayTime = 0.5s;
	Duration<Microseconds> decayTime = 0.5s;

	bool pulse = false;
	bool pulseBackground = false;
	Duration<Microseconds> pulseTime = 0.1s;

	bool strobe = false;
	float strobeIntensity = 0.66f;

	std::optional<bool> rotating = std::nullopt;
	float rotationSpeed = 1.0f;

	std::optional<bool> blur = std::nullopt;
	GLenum sourceFactor = GL_ONE;
	GLenum destFactor = GL_ZERO;
	GLenum sourceAlphaFactor = GL_ONE;
	GLenum destAlphaFactor = GL_ZERO;
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

	int fftSize = 8192;

	bool availableInMiniPlayer = false;

	DynamicGain<float> dynamicGain { 0 };
};