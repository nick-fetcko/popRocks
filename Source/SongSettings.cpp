#include "SongSettings.hpp"

#include <fstream>

#include "Utils/Filesystem.hpp"

using namespace Fetcko;

void SongSettings::Load() {
	if (const auto path = Filesystem::GetPath("SongSettings.json"); !path.empty() && std::filesystem::exists(path)) {
		std::ifstream inFile(path);

		Node json;
		json.parseStream<Json>(inFile);

		json >> settings;
	}
}

void SongSettings::SetPreferredAlbumArt(const std::filesystem::path &path, const std::string &art) {
	const auto pathString = path.generic_u8string();

	if (const auto iter = settings.find(pathString); iter != settings.end())
		iter->second.albumArt = art;
	else
		settings[pathString] = SongSetting{art, false, false};

	Save();
}

void SongSettings::SetHalveDetected(const std::filesystem::path &path, bool halveDetected) {
	const auto pathString = path.generic_u8string();

	if (const auto iter = settings.find(pathString); iter != settings.end()) {
		iter->second.halveDetected = halveDetected;

		// If we're back to default settings, remove this setting
		if (iter->second.albumArt.empty() && !iter->second.halveDetected && !iter->second.useOtherHalf)
			settings.erase(iter);
	} else
		settings[pathString] = SongSetting{ "", halveDetected, false };

	Save();
}

void SongSettings::SetUseOtherHalf(const std::filesystem::path &path, bool useOtherHalf) {
	const auto pathString = path.generic_u8string();

	if (const auto iter = settings.find(pathString); iter != settings.end()) {
		iter->second.useOtherHalf = useOtherHalf;

		// If we're back to default settings, remove this setting
		if (iter->second.albumArt.empty() && !iter->second.halveDetected && !iter->second.useOtherHalf)
			settings.erase(iter);
	} else
		settings[pathString] = SongSetting{ "", false, useOtherHalf };

	Save();
}

const bool SongSettings::HasSettings(const std::filesystem::path &path) const {
	return settings.find(path.generic_u8string()) != settings.end();
}

SongSettings::SongSetting *SongSettings::GetSettings(const std::filesystem::path &path) {
	const auto iter = settings.find(path.generic_u8string());

	if (iter == settings.end()) return nullptr;
	else return &(iter->second);
}

void SongSettings::RemoveSetting(const std::filesystem::path &path) {
	const auto iter = settings.find(path.generic_u8string());

	if (iter != settings.end())
		settings.erase(iter);

	Save();
}

void SongSettings::Save() {
	if (const auto path = Filesystem::GetPath("SongSettings.json"); !path.empty()) {
		std::ofstream outFile(path);

		Node json;
		json << settings;

		json.writeStream<Json>(outFile, NodeFormat::Beautified);
	}
}

const Node &operator>>(const Node &node, SongSettings::SongSetting &setting) {
	node["albumArt"]->get(setting.albumArt);
	node["halveDetected"]->get(setting.halveDetected);
	node["useOtherHalf"]->get(setting.useOtherHalf);

	return node;
}
Node &operator<<(Node &node, const SongSettings::SongSetting &setting) {
	node["albumArt"]->set(setting.albumArt);
	node["halveDetected"]->set(setting.halveDetected);
	node["useOtherHalf"]->set(setting.useOtherHalf);

	return node;
}
