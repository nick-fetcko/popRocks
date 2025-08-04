#pragma once

#include <filesystem>

#include <SDL_video.h>

#include "MathCPP/Duration.hpp"

#include "Serial/Json.hpp"

#include "Utils/Logger.hpp"

using namespace std::chrono_literals;

using namespace Fetcko;
using namespace MathsCPP;
using namespace serial;

class Settings {
public:
	struct ColorSelection {
		double minPercentage = 0.02;
		double minSaturation = 0.1;
		double minValue = 0.25;
		double minHueSeparation = 25.0;
		double minValueSeparation = 0.1;
		double minRgbSeparation = 0.70;
	};

	static Settings settings;

	const float &GetVolume() const { return volume; }
	void SetVolume(float volume);

	const bool &GetExclsuive() const { return exclusive; }
	void SetExclusive(bool exclusive);

	const ColorSelection &GetColorSelection() const { return colorSelection; }
	void SetColorSelection(ColorSelection colorSelection);

	const int &GetWindowWidth() const { return windowWidth; }
	void SetWindowWidth(int windowWidth);

	const int &GetWindowHeight() const { return windowHeight; }
	void SetWindowHeight(int windowHeight);

	const int &GetWindowX() const { return windowX; }
	void SetWindowX(int windowX);

	const int &GetWindowY() const { return windowY; }
	void SetWindowY(int windowY);

	const std::size_t &GetBufferLength() { return bufferLength; }
	void SetBufferLength(std::size_t bufferLength);

	const Duration<Microseconds> &GetDecayTime() { return decayTime; }
	void SetDecayTime(Duration<Microseconds> decayTime);

	const Duration<Microseconds> &GetFadeTime() { return fadeTime; }
	void SetFadeTime(Duration<Microseconds> fadeTime);

	const bool &GetPulse() const { return pulse; }
	void SetPulse(bool pulse);

	const Duration<Microseconds> &GetPulseTime() { return pulseTime; }
	void SetPulseTime(Duration<Microseconds> pulseTime);

	friend const Node &operator>>(const Node &node, Settings &settings);
	friend Node &operator<<(Node &node, const Settings &settings);

	friend const Node &operator>>(const Node &node, ColorSelection &colorSelection);
	friend Node &operator<<(Node &node, const ColorSelection &colorSelection);
private:
	static std::filesystem::path GetPath();

	static Settings Load();

	void Save();

	// Actual settings
	float volume = 1.0f;
	bool exclusive = true;
	ColorSelection colorSelection;

	int windowWidth = 1920;
	int windowHeight = 1080;

	int windowX = SDL_WINDOWPOS_CENTERED;
	int windowY = SDL_WINDOWPOS_CENTERED;

	std::size_t bufferLength = 2048;
	
	Duration<Microseconds> decayTime = 0.5s;
	Duration<Microseconds> fadeTime = 0.5s;

	bool pulse = false;
	Duration<Microseconds> pulseTime = 0.1s;
};