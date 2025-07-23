#pragma once

#include <filesystem>

#include "Serial/Json.hpp"

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
};