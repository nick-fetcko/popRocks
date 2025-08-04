#pragma once

#include <filesystem>

#include <SDL_video.h>

#include "Serial/Json.hpp"
#include "Utils/Logger.hpp"

using namespace Fetcko;
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
};