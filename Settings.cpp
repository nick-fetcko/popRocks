#include "Settings.hpp"

#include <fstream>

#ifdef _WIN32
#include <Shlobj.h>
#endif

#include "CConsole.h"

Settings Settings::settings = Settings::Load();

std::filesystem::path Settings::GetPath() {
	std::filesystem::path ret;

#ifdef _WIN32
	PWSTR folder;
	if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &folder) == S_OK) {
		ret = std::filesystem::path(folder);

		CoTaskMemFree(folder);
	}
#elif defined(__linux__)
	ret = "~/.config";
#endif

	if (!ret.empty()) {
		ret /= "Fetcko";
		if (!std::filesystem::exists(ret))
			std::filesystem::create_directory(ret);

		ret /= "popRocks";
		if (!std::filesystem::exists(ret))
			std::filesystem::create_directory(ret);

		ret /= "Settings.json";
	}

	return ret;
}

Settings Settings::Load() {
	Settings ret;

	if (auto path = GetPath(); !path.empty()) {
		std::ifstream inFile(path, std::ios::in);

		try {
			Node json;
			json.parseStream<Json>(inFile);

			json >> ret;

			// Any setting not loaded will be set
			// to its default value. Drop those
			// default values down right away
			// so the user can edit them if
			// they'd like to.
			ret.Save();
		} catch (std::exception &e) {
			CConsole::Console.Print(std::string("Could not load settings due to ") + e.what(), MSG_ALERT);
		}
	}

	return ret;
}

void Settings::SetVolume(float volume) {
	this->volume = volume;
	Save();
}

void Settings::SetExclusive(bool exclusive) {
	this->exclusive = exclusive;
	Save();
}

void Settings::SetColorSelection(ColorSelection colorSelection) {
	this->colorSelection = colorSelection;
	Save();
}

void Settings::Save() {
	if (auto path = GetPath(); !path.empty()) {
		std::ofstream outFile(path, std::ios::out);

		Node json;
		json << *this;

		json.writeStream<Json>(outFile, NodeFormat::Beautified);
	}
}

const Node &operator>>(const Node &node, Settings::ColorSelection &colorSelection) {
	if (node.has("minPercentage"))
		node["minPercentage"]->get(colorSelection.minPercentage);

	if (node.has("minHueSeparation"))
		node["minHueSeparation"]->get(colorSelection.minHueSeparation);

	if (node.has("minValueSeparation"))
		node["minValueSeparation"]->get(colorSelection.minValueSeparation);

	if (node.has("minRgbSeparation"))
		node["minRgbSeparation"]->get(colorSelection.minRgbSeparation);

	if (node.has("minSaturation"))
		node["minSaturation"]->get(colorSelection.minSaturation);

	if (node.has("minValue"))
		node["minValue"]->get(colorSelection.minValue);

	return node;
}

Node &operator<<(Node &node, const Settings::ColorSelection &colorSelection) {
	node["minPercentage"]->set(colorSelection.minPercentage);
	node["minHueSeparation"]->set(colorSelection.minHueSeparation);
	node["minValueSeparation"]->set(colorSelection.minValueSeparation);
	node["minRgbSeparation"]->set(colorSelection.minRgbSeparation);
	node["minSaturation"]->set(colorSelection.minSaturation);
	node["minValue"]->set(colorSelection.minValue);

	return node;
}

const Node &operator>>(const Node &node, Settings &settings) {
	if (node.has("volume"))
		node["volume"]->get(settings.volume);

	if (node.has("exclusive"))
		node["exclusive"]->get(settings.exclusive);

	if (node.has("colorSelection"))
		node["colorSelection"]->get(settings.colorSelection);

	return node;
}

Node &operator<<(Node &node, const Settings &settings) {
	node["volume"]->set(settings.volume);
	node["exclusive"]->set(settings.exclusive);
	node["colorSelection"]->set(settings.colorSelection);

	return node;
}